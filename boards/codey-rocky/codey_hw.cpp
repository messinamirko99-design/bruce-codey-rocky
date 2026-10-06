#include "codey_hw.h"

#include <Wire.h>
#include <display/codey_fonts.h>  // authentic fonts + animations
#ifdef USE_LEDMATRIX
#include <display/ledmatrix.h>
#endif

// same bit-reverse the original applies to "software" oriented font data
static uint8_t lm_rev8_local(uint8_t d) {
    d = (uint8_t)(((d & 0xF0) >> 4) | ((d & 0x0F) << 4));
    d = (uint8_t)(((d & 0xCC) >> 2) | ((d & 0x33) << 2));
    d = (uint8_t)(((d & 0xAA) >> 1) | ((d & 0x55) << 1));
    return d;
}

/*********************************************************************
** Makeblock Codey Rocky - hardware drivers for the Bruce port.
** Faithfully reimplemented from Makeblock's MicroPython source
** (FFtust/micropython_esp32_local, makeblock/src/hardware/onboard/*)
** following the implementation guide: original sampling algorithms,
** register sequences and conversion constants.
*********************************************************************/

namespace CodeyHW {

/*********************************************************************
** ADC common setup (all four analog inputs are on ADC1, WiFi-safe)
** - 12 bit resolution (0-4095)
** - 11 dB attenuation, like the original firmware configuration
*********************************************************************/
static bool adc_ready = false;

static void adcInit() {
    if (adc_ready) return;
    analogReadResolution(12);
    analogSetPinAttenuation(CODEY_LIGHT_PIN, ADC_11db);
    analogSetPinAttenuation(CODEY_SOUND_PIN, ADC_11db);
    analogSetPinAttenuation(CODEY_KNOB_PIN, ADC_11db);
    analogSetPinAttenuation(BAT_PIN, ADC_11db);
    adc_ready = true;
}

// Original denoising algorithm: 10 readings, discard max and min,
// average the remaining 8 (codey_*_sensor_board.c)
static float trimmedMean10(int pin) {
    adcInit();
    int readings[10];
    for (int i = 0; i < 10; i++) readings[i] = analogRead(pin);
    int sum = 0, mx = readings[0], mn = readings[0];
    for (int i = 0; i < 10; i++) {
        sum += readings[i];
        if (readings[i] > mx) mx = readings[i];
        if (readings[i] < mn) mn = readings[i];
    }
    return (float)(sum - mx - mn) / 8.0f;
}

int lightSensor() {
    // original: value_percent = mean * (100.0 / 4095.0)
    int v = (int)(trimmedMean10(CODEY_LIGHT_PIN) * (100.0f / 4095.0f));
    return v > 100 ? 100 : (v < 0 ? 0 : v);
}

int soundSensor() {
    // Original mic path: peak-to-peak amplitude over ~50 ms window, scaled 0-100
    adcInit();
    int mn = 4095, mx = 0;
    unsigned long t0 = millis();
    while (millis() - t0 < 50) {
        int s = analogRead(CODEY_SOUND_PIN);
        if (s < mn) mn = s;
        if (s > mx) mx = s;
        yield();
    }
    int pp = mx - mn;
    int v = (int)(pp * (100.0f / 4095.0f));
    return v > 100 ? 100 : (v < 0 ? 0 : v);
}

int knob() {
    // original returns the raw mean; normalize with the same 100/4095 factor
    int v = (int)(trimmedMean10(CODEY_KNOB_PIN) * (100.0f / 4095.0f));
    return v > 100 ? 100 : (v < 0 ? 0 : v);
}

/*********************************************************************
** MPU6050 (I2C on SDA=19 / SCL=18, addr 0x68)
** Original init sequence and conversion constants. The chip Makeblock
** shipped answers 0x98 on WHO_AM_I (custom ID) - stock MPU6050 answers
** 0x68; the register map is identical either way.
*********************************************************************/
#define MPU_ADDR 0x68

static bool imu_ok = false;
static float gyr_off_x = 0, gyr_off_y = 0, gyr_off_z = 0;
// original low-pass filter state (exponential, alpha 0.2), axes inverted
static float acc_fx = 0, acc_fy = 0, acc_fz = 0;
static bool acc_filtered = false;

static bool mpuWrite(uint8_t reg, uint8_t val) {
    Wire1.beginTransmission(MPU_ADDR);
    Wire1.write(reg);
    Wire1.write(val);
    return Wire1.endTransmission() == 0;
}

bool imuBegin() {
    if (imu_ok) return true;
    Wire1.begin(SDA, SCL, 100000);

    // WHO_AM_I: original expects 0x98, retry up to 5 times x 20ms
    bool id_ok = false;
    for (int i = 0; i < 5; i++) {
        Wire1.beginTransmission(MPU_ADDR);
        Wire1.write(0x75);
        if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom(MPU_ADDR, 1) == 1) {
            uint8_t id = Wire1.read();
            if (id == 0x98 || id == 0x68) { // custom clone or stock MPU6050
                id_ok = true;
                break;
            }
        }
        delay(20);
    }
    if (!id_ok) return false; // no IMU on the bus

    // init sequence, faithful to codey_gyro_board.c (with its delays)
    mpuWrite(0x6B, 0x00); // wake up
    delay(200);
    mpuWrite(0x1A, 0x01); // DLPF
    delay(100);
    mpuWrite(0x1B, 0x08); // gyro +-500 dps (65.5 LSB/deg/s)
    delay(100);
    mpuWrite(0x19, 19);   // ~50 Hz sample rate

    // gyro offset calibration: 500 samples, discard the first and last 50
    long sx = 0, sy = 0, sz = 0;
    int n = 0;
    for (int i = 0; i < 500; i++) {
        Wire1.beginTransmission(MPU_ADDR);
        Wire1.write(0x3B);
        if (Wire1.endTransmission(false) == 0 && Wire1.requestFrom(MPU_ADDR, 14) == 14) {
            for (int b = 0; b < 8; b++) Wire1.read(); // skip accel + temp
            int hi = Wire1.read(), lo = Wire1.read();
            int hi2 = Wire1.read(), lo2 = Wire1.read();
            int hi3 = Wire1.read(), lo3 = Wire1.read();
            if (i >= 50 && i < 450) {
                sx += (int16_t)((hi << 8) | lo);
                sy += (int16_t)((hi2 << 8) | lo2);
                sz += (int16_t)((hi3 << 8) | lo3);
                n++;
            }
        }
    }
    if (n > 0) {
        gyr_off_x = (float)sx / n;
        gyr_off_y = (float)sy / n;
        gyr_off_z = (float)sz / n;
    }
    acc_filtered = false;
    imu_ok = true;
    return true;
}

void imuRead(
    float &ax, float &ay, float &az, float &gx, float &gy, float &gz, float &tempC
) {
    ax = ay = az = gx = gy = gz = tempC = 0;
    if (!imuBegin()) return;
    Wire1.beginTransmission(MPU_ADDR);
    Wire1.write(0x3B);
    if (Wire1.endTransmission(false) != 0) return;
    if (Wire1.requestFrom(MPU_ADDR, 14) != 14) return;
    auto rd16 = []() -> int16_t {
        int hi = Wire1.read();
        int lo = Wire1.read();
        return (int16_t)((hi << 8) | lo);
    };
    int16_t rawAx = rd16(), rawAy = rd16(), rawAz = rd16();
    int16_t rawT = rd16();
    int16_t rawGx = rd16(), rawGy = rd16(), rawGz = rd16();

    // gyro: (raw - calibration offset) / 65.5 -> deg/s
    gx = (rawGx - gyr_off_x) / 65.5f;
    gy = (rawGy - gyr_off_y) / 65.5f;
    gz = (rawGz - gyr_off_z) / 65.5f;

    // temperature: T = 36.53 + raw / 340
    tempC = 36.53f + rawT / 340.0f;

    // original axis convention ("the direction is invert to the definition
    // of codey"): acc_x = -raw, acc_y = -raw, acc_z = raw (in g)
    float axf = -rawAx / 16384.0f;
    float ayf = -rawAy / 16384.0f;
    float azf = rawAz / 16384.0f;

    // exponential filter (alpha 0.2) used by the original for the angles
    if (!acc_filtered) {
        acc_fx = axf;
        acc_fy = ayf;
        acc_fz = azf;
        acc_filtered = true;
    } else {
        acc_fx = acc_fx * 0.8f + axf * 0.2f;
        acc_fy = acc_fy * 0.8f + ayf * 0.2f;
        acc_fz = acc_fz * 0.8f + azf * 0.2f;
    }
    ax = axf;
    ay = ayf;
    az = azf;
}

void imuAngles(float &pitch, float &roll) {
    float ax, ay, az, gx, gy, gz, t;
    imuRead(ax, ay, az, gx, gy, gz, t);
    // original angle math (on the filtered, axis-inverted values)
    float sgn = (acc_fz >= 0.0f) ? 1.0f : -1.0f;
    pitch = atan2f(-acc_fx, sgn * sqrtf(acc_fy * acc_fy + acc_fz * acc_fz)) * 57.29578f;
    roll = atan2f(acc_fy, sgn * sqrtf(acc_fx * acc_fx + acc_fz * acc_fz)) * 57.29578f;
    // "the direction definition of roll is invert to codey"
    roll = -roll;
}

/*********************************************************************
** RGB LED: single RGB LED driven by LEDC PWM on R=14 / G=21 / B=4.
** Verified in the original source (codey_rgbled_board.h/.c): it is a
** 3-channel PWM LED, NOT a WS2812 strip - the RMT peripheral there is
** used only by the NEC infrared driver. The red channel duty is
** inverted (MAX_DUTY - duty) and intensities are 0-100.
*********************************************************************/
void rgbSet(uint8_t r, uint8_t g, uint8_t b) {
    // map 0-255 input onto the original 0-100 intensity range
    uint8_t ri = (uint16_t)r * 100 / 255;
    uint8_t gi = (uint16_t)g * 100 / 255;
    uint8_t bi = (uint16_t)b * 100 / 255;
    analogWrite(CODEY_RGB_R, 255 - (ri * 255) / 100); // inverted red, like the original
    analogWrite(CODEY_RGB_G, (gi * 255) / 100);
    analogWrite(CODEY_RGB_B, (bi * 255) / 100);
}

void rgbOff() { rgbSet(0, 0, 0); }

/*********************************************************************
** Speaker: ESP32 internal DAC channel 1 = GPIO25 (original "8bit
** voice" path: dac_output_enable + dac_output_voltage). Tones are
** generated as a square wave written to the DAC.
*********************************************************************/
void speakerTone(uint32_t freq, uint32_t durationMs) {
    if (freq == 0) return;
    uint32_t halfUs = 500000UL / freq; // half period in microseconds
    if (halfUs < 20) halfUs = 20;
    unsigned long start = millis();
    bool high = true;
    while (millis() - start < durationMs) {
        dacWrite(BUZZ_PIN, high ? 200 : 20);
        high = !high;
        delayMicroseconds(halfUs);
    }
    dacWrite(BUZZ_PIN, 0);
}

void speakerNoTone() { dacWrite(BUZZ_PIN, 0); }

/*********************************************************************
** Authentic matrix faces (happy/cry/angry/... from the 2018 firmware)
*********************************************************************/
int faceCount() { return ANIMATIONS_LEN; }

const char *faceName(int index) {
    if (index < 0 || index >= ANIMATIONS_LEN) return "?";
    return ANIMATIONS[index].name;
}

void playFace(int index) {
    if (index < 0 || index >= ANIMATIONS_LEN) return;
    const AnimDef &a = ANIMATIONS[index];
    uint8_t loops = a.loops < 1 ? 1 : 1; // one loop per selection
    for (uint8_t l = 0; l < loops; l++) {
        for (uint16_t f = 0; f < a.frames; f++) {
            uint8_t out[16];
            for (int x = 0; x < 16; x++) out[x] = lm_rev8_local(a.data[f][x]);
#ifdef USE_LEDMATRIX
            LedMatrix::pushFrame(out);
#endif
            delay(a.interval_ms);
        }
    }
}

/*********************************************************************
** Rocky base: Neurons protocol over UART1 (TX=17 RX=16 @115200)
**
** Frame: F0 | dev_id | service | [sub] | params... | sum | F7
** sum = (sum of bytes from dev_id to last param) & 0x7f
** Discovery: heartbeat F0 FF 10 00 0F F7 every 500ms; the base answers
** with F0 <id> 10 63 10 <sum> F7 announcing its assigned device id.
** ROCKY block = service 0x63, sub 0x10. Motor commands (from the
** firmware lib table): 0x0c stop, 0x0d fwd, 0x0e back, 0x0f right,
** 0x10 left (1 byte speed 0-100), 0x11 drive(4 bytes dir/pwr/dir/pwr).
*********************************************************************/
static HardwareSerial &rockySerial = Serial1;
static bool rocky_started = false;
static int rocky_dev = -1;
static unsigned long last_heartbeat = 0;
static int s_grey = -1, s_color = -1, s_refl_l = -1, s_refl_r = -1, s_obstacle = -1;

static void rockySendFrame(uint8_t dev, uint8_t service, uint8_t hasSub, const uint8_t *params, size_t n) {
    uint8_t buf[24];
    size_t i = 0;
    buf[i++] = 0xF0;
    buf[i++] = dev;
    buf[i++] = service;
    if (hasSub) buf[i++] = 0x10; // BLOCK_CODEY_CAR
    for (size_t k = 0; k < n && i < sizeof(buf) - 2; k++) buf[i++] = params[k];
    uint8_t sum = 0;
    for (size_t k = 1; k < i; k++) sum += buf[k];
    buf[i++] = sum & 0x7f;
    buf[i++] = 0xF7;
    rockySerial.write(buf, i);
}

static void rockyCmd(uint8_t cmd, const uint8_t *params = nullptr, size_t n = 0) {
    if (rocky_dev < 0) return;
    uint8_t p[8];
    p[0] = cmd;
    if (params && n) memcpy(p + 1, params, n > 7 ? 7 : n);
    rockySendFrame((uint8_t)rocky_dev, 0x63, 1, p, (n > 7 ? 7 : n) + 1);
}

static void rockyHeartbeat() {
    uint8_t p[1] = {0x00};
    rockySendFrame(0xFF, 0x10, 0, p, 1); // CTL_ASSIGN_DEV_ID, no sub-service
}

bool rockyBegin() {
    if (rocky_started) return true;
    // ESP32 HardwareSerial: begin(baud, config, rxPin, txPin)
    rockySerial.begin(115200, SERIAL_8N1, ROCKY_UART_RX, ROCKY_UART_TX);
    rocky_started = true;
    rocky_dev = -1;
    s_grey = s_color = s_refl_l = s_refl_r = s_obstacle = -1;
    last_heartbeat = 0;
    while (rockySerial.available()) rockySerial.read(); // flush junk
    // kick discovery immediately
    rockyHeartbeat();
    return true;
}

bool rockyFound() { return rocky_dev >= 0; }

void rockyTick() {
    rockyBegin();
    unsigned long now = millis();
    if (now - last_heartbeat >= 500) {
        last_heartbeat = now;
        rockyHeartbeat();
    }
    // parse Neurons frames: F0 | dev | service | ... | sum | F7
    // sum = (sum of bytes from dev through last param) & 0x7f
    static uint8_t rbuf[48];
    static size_t rlen = 0;
    while (rockySerial.available()) {
        uint8_t c = rockySerial.read();
        if (rlen == 0 && c != 0xF0) continue;
        if (rlen < sizeof(rbuf)) rbuf[rlen++] = c;
        if (c == 0xF7 && rlen >= 4) {
            // verify checksum (byte before F7)
            uint8_t got = rbuf[rlen - 2];
            uint8_t sum = 0;
            for (size_t k = 1; k < rlen - 2; k++) sum = (uint8_t)(sum + rbuf[k]);
            sum &= 0x7f;
            if (sum == got) {
                uint8_t dev = rbuf[1];
                uint8_t service = rbuf[2];
                // Discovery: F0 <id> 10 63 10 ... F7  (CTL reply with Rocky type)
                if (service == 0x10 && rlen >= 6 && rbuf[3] == 0x63 && rbuf[4] == 0x10) {
                    if (rocky_dev < 0 || rocky_dev != (int)dev) {
                        rocky_dev = (int)dev;
                        rockyStop();
                        rockyRequestReports();
                    }
                }
                // Sensor / status from Rocky block service 0x63 sub 0x10
                else if (service == 0x63 && rlen >= 6 && rbuf[3] == 0x10) {
                    if (rocky_dev < 0) rocky_dev = (int)dev; // learn id from reports too
                    if (dev == (uint8_t)rocky_dev) {
                        uint8_t cmd = rbuf[4];
                        // values sit between cmd and checksum
                        int nvals = (int)rlen - 6;
                        auto val = [&](int idx) -> int {
                            int p = 5 + idx;
                            return (idx >= 0 && idx < nvals) ? (int)rbuf[p] : -1;
                        };
                        switch (cmd) {
                            case 0x02: s_color = val(0); break;
                            case 0x04: s_grey = val(0); break;
                            case 0x05: s_refl_l = val(0); if (nvals > 1) s_refl_r = val(1); break;
                            case 0x06: s_refl_r = val(0); break;
                            case 0x08: s_obstacle = val(0); break;
                            default: break;
                        }
                    }
                }
            }
            rlen = 0;
        } else if (rlen >= sizeof(rbuf)) {
            rlen = 0;
        }
    }
}

void rockyStop() { rockyCmd(0x0c); }

void rockyDrive(int leftDir, int leftPwr, int rightDir, int rightPwr) {
    uint8_t p[4];
    p[0] = (uint8_t)(leftDir ? 1 : 0);
    p[1] = (uint8_t)constrain(leftPwr, 0, 100);
    p[2] = (uint8_t)(rightDir ? 1 : 0);
    p[3] = (uint8_t)constrain(rightPwr, 0, 100);
    rockyCmd(0x11, p, 4);
}

void rockyForward(int speed) {
    uint8_t p[1];
    p[0] = (uint8_t)constrain(speed, 0, 100);
    rockyCmd(0x0d, p, 1);
}

void rockyBack(int speed) {
    uint8_t p[1];
    p[0] = (uint8_t)constrain(speed, 0, 100);
    rockyCmd(0x0e, p, 1);
}

void rockyLeft(int speed) {
    uint8_t p[1];
    p[0] = (uint8_t)constrain(speed, 0, 100);
    rockyCmd(0x10, p, 1);
}

void rockyRight(int speed) {
    uint8_t p[1];
    p[0] = (uint8_t)constrain(speed, 0, 100);
    rockyCmd(0x0f, p, 1);
}

int rockyGrey() { return s_grey; }
int rockyColor() { return s_color; }
int rockyReflectL() { return s_refl_l; }
int rockyReflectR() { return s_refl_r; }
int rockyObstacle() { return s_obstacle; }

void rockyRequestReports() {
    if (rocky_dev < 0) return;
    // set report mode frame: F0 dev 63 10 7F <sensor> <mode> <period LONG_40 x5> sum F7
    uint8_t sensors[4] = {0x04, 0x05, 0x08, 0x02}; // grey, reflect, barrier, color
    for (int i = 0; i < 4; i++) {
        uint8_t p[7];
        p[0] = sensors[i];
        p[1] = 2; // periodic
        uint32_t period = 200;
        p[2] = period & 0x7f;
        p[3] = (period >> 7) & 0x7f;
        p[4] = 0;
        p[5] = 0;
        p[6] = 0;
        rockyCmd(0x7f, p, 7);
    }
}

} // namespace CodeyHW
