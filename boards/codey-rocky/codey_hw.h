#pragma once
#include <Arduino.h>

/*********************************************************************
** Makeblock Codey Rocky - hardware drivers for the Bruce port.
** Reverse engineered from Makeblock's MicroPython port
** (FFtust/micropython_esp32_local, makeblock/src/hardware/).
*********************************************************************/

namespace CodeyHW {

// ---- ADC sensors (scaled 0-100 like the original firmware) ----
int lightSensor(); // GPIO35
int soundSensor(); // GPIO36, peak-to-peak over a ~50ms window
int knob();        // GPIO39

// ---- MPU6050 IMU (I2C1: SDA=19 SCL=18, addr 0x68) ----
bool imuBegin();
void imuRead(
    float &ax, float &ay, float &az,   // g
    float &gx, float &gy, float &gz,   // deg/s
    float &tempC
);
void imuAngles(float &pitch, float &roll); // accel angles in degrees

// ---- Onboard RGB LED (PWM: R=14 inverted, G=21, B=4) ----
void rgbSet(uint8_t r, uint8_t g, uint8_t b);
void rgbOff();

// ---- Speaker (square wave on DAC pin 25 via LEDC) ----
void speakerTone(uint32_t freq, uint32_t durationMs);
void speakerNoTone();

// ---- Authentic matrix faces (from the original firmware, hardware-tested
// fonts/animations ported from the working codey-sniffer project) ----
int faceCount();
const char *faceName(int index);
void playFace(int index); // plays one loop, blocking (~1-2 s)

// ---- Rocky base co-processor (Neurons protocol, UART1 115200 TX=17 RX=16) ----
bool rockyBegin();         // init UART, call rockyTick() afterwards
void rockyTick();          // feed heartbeat + parse responses (call ~every loop)
bool rockyFound();         // co-processor discovered and addressed
void rockyStop();
void rockyForward(int speed); // speed 0-100
void rockyBack(int speed);
void rockyLeft(int speed);
void rockyRight(int speed);
void rockyDrive(int leftDir, int leftPwr, int rightDir, int rightPwr);

// sensor values reported by the base (periodic reports, -1 = unknown yet)
int rockyGrey();        // 0x04
int rockyColor();       // 0x02 (color index, see Makeblock color_dict)
int rockyReflectL();    // 0x05 (line reflectance left)
int rockyReflectR();    // 0x06 (line reflectance right)
int rockyObstacle();    // 0x08 (0/1)
void rockyRequestReports(); // enable periodic sensor reporting

} // namespace CodeyHW
