#include "core/powerSave.h"
#include <interface.h>
#include <globals.h>
#include <Arduino.h>

#ifdef USE_LEDMATRIX
#include <display/ledmatrix.h>
#endif

#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_periph.h"
#include <esp_sleep.h>

/*********************************************************************
** Makeblock Codey Rocky - board interface
** A=2 (active HIGH) Select | B=12 (active HIGH) Next | C=27 (active LOW) Prev
** Power key=34 (active LOW): short = Esc, hold 2s = power off
** Power latch = 15 must be held HIGH as soon as possible at boot.
*********************************************************************/

// Hold the power latch HIGH before anything else runs (global ctors run
// before Arduino setup()), otherwise the device switches off when the
// power key is released.
__attribute__((constructor)) static void codey_early_power_latch() {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[15], PIN_FUNC_GPIO);
    REG_SET_BIT(GPIO_ENABLE_W1TS_REG, 1ULL << 15);
    REG_SET_BIT(GPIO_OUT_W1TS_REG, 1ULL << 15);
}

/***************************************************************************************
** Function name: _setup_gpio()
** Description:   initial setup for the device
***************************************************************************************/
void _setup_gpio() {
    // keep the power latch asserted (belt and suspenders)
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);

    pinMode(SEL_BTN, INPUT);      // external pull-down, active HIGH
    pinMode(DW_BTN, INPUT);       // external pull-down, active HIGH
    pinMode(UP_BTN, INPUT_PULLUP); // external pull-up, active LOW
    pinMode(PWR_BTN, INPUT);      // input-only power key, active LOW

    // Onboard RGB status LED (common anode PWM driven), start off
    pinMode(CODEY_RGB_R, OUTPUT);
    pinMode(CODEY_RGB_G, OUTPUT);
    pinMode(CODEY_RGB_B, OUTPUT);
    digitalWrite(CODEY_RGB_R, LOW);
    digitalWrite(CODEY_RGB_G, LOW);
    digitalWrite(CODEY_RGB_B, LOW);

    // boot-strapping safety: fixed initial states (see Makeblock porting notes)
    pinMode(0, INPUT);
    // GPIO5 = IR RX on Codey: leave floating as INPUT so the receiver module
    // can drive it. Do NOT enable internal pull-up (would fight the IR demod).
    pinMode(5, INPUT);
}

/***************************************************************************************
** Function name: _post_setup_gpio()
***************************************************************************************/
void _post_setup_gpio() {}

/***************************************************************************************
** Function name: getBattery()
** Description:   Delivers the battery value from 1-100
** Original firmware: vol = counts * ((1/4096) * 1.1 * 3.6 * 2) + 0.2.
** analogReadMilliVolts() already returns the calibrated pin voltage (the
** 1.1 * 3.6 full-scale is accounted for), so: V_batt = pin_mV/1000 * 2 + 0.2.
** Sampling uses the same 10-read/trimmed-mean scheme as the other sensors.
***************************************************************************************/
int getBattery() {
    uint32_t readings[10];
    uint32_t sum = 0, mx = 0, mn = 0xFFFFFFFF;
    for (int i = 0; i < 10; i++) {
        readings[i] = analogReadMilliVolts(BAT_PIN);
        sum += readings[i];
        if (readings[i] > mx) mx = readings[i];
        if (readings[i] < mn) mn = readings[i];
    }
    uint32_t avg_mv = (sum - mx - mn) / 8;
    float volts = (avg_mv / 1000.0f) * 2.0f + 0.2f; // original conversion chain
    // linear approximation of the original voltage->percent table:
    // 0% at ~3.5 (3.3V real), 100% at ~4.4 (4.2V real), in "vol" units
    int percent = (int)((volts - 3.5f) / (4.4f - 3.5f) * 100.0f);
    return (percent <= 0) ? 1 : (percent >= 100) ? 100 : percent;
}

/*********************************************************************
** Function: setBrightness -> matrix brightness (8 levels)
**********************************************************************/
void _setBrightness(uint8_t brightval) {
    uint8_t level = ((uint16_t)brightval * 8 + 50) / 100;
#ifdef USE_LEDMATRIX
    LedMatrix::setBrightness(level);
#endif
}

/*********************************************************************
** Function: InputHandler
**********************************************************************/
void InputHandler(void) {
    static unsigned long tm = 0;
    if (millis() - tm < 180 && !LongPress) return;

    bool selPressed = (digitalRead(SEL_BTN) == HIGH);
    bool dwPressed = (digitalRead(DW_BTN) == HIGH);
    bool upPressed = (digitalRead(UP_BTN) == LOW);   // C, active LOW
    bool pwrPressed = (digitalRead(PWR_BTN) == LOW); // power, active LOW

    // ---- Power key: short press on RELEASE = Esc, hold 2s = power off ----
    static unsigned long pwrSince = 0;
    static bool pwrWas = false;
    bool pwrShort = false;
    if (pwrPressed && !pwrWas) {
        pwrSince = millis();
        pwrWas = true;
    } else if (!pwrPressed && pwrWas) {
        // released
        if (millis() - pwrSince < 2000) pwrShort = true;
        pwrWas = false;
    }
    if (pwrPressed && pwrWas && (millis() - pwrSince > 2000)) {
        powerOff(); // does not return
        return;
    }

    // ---- Button C (UP): short = Prev, hold ~700 ms = Esc/Back ----
    static unsigned long upSince = 0;
    static bool upWas = false;
    static bool upLongFired = false;
    bool upShort = false;
    bool upLong = false;
    if (upPressed && !upWas) {
        upSince = millis();
        upWas = true;
        upLongFired = false;
    } else if (upPressed && upWas && !upLongFired && (millis() - upSince > 700)) {
        upLong = true;
        upLongFired = true;
        LongPress = true;
    } else if (!upPressed && upWas) {
        if (!upLongFired && (millis() - upSince < 700)) upShort = true;
        upWas = false;
        upLongFired = false;
        LongPress = false;
    }

    bool anyPressed = selPressed || dwPressed || upPressed || pwrShort || upLong;
    if (anyPressed) tm = millis();
    if (anyPressed && wakeUpScreen()) return;

    AnyKeyPress = anyPressed;
    // Prev/Up only on short press of C (not while held for long-Esc)
    PrevPress = upShort;
    UpPress = upShort;
    NextPress = dwPressed;
    DownPress = dwPressed;
    EscPress = pwrShort || upLong;
    SelPress = selPressed;
}

/*********************************************************************
** Function: keyboard (no physical keyboard: keep text, edit via WebUI)
**********************************************************************/
String keyboard(String mytext, int maxSize, String msg) { return mytext; }

/*********************************************************************
** Function: powerOff
**********************************************************************/
void powerOff() {
#ifdef USE_LEDMATRIX
    LedMatrix::hardwareOff();
#endif
    // release the power latch
    digitalWrite(15, LOW);
    delay(250);
    // still alive (USB powered)? then at least deep sleep
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PWR_BTN, 0); // wake on power key press
    esp_deep_sleep_start();
}

/*********************************************************************
** Function: goToDeepSleep
**********************************************************************/
void goToDeepSleep() {
#ifdef USE_LEDMATRIX
    LedMatrix::hardwareOff();
#endif
    digitalWrite(15, LOW); // cut the power latch too (same as power off)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PWR_BTN, 0);
    esp_deep_sleep_start();
}

/*********************************************************************
** Function: checkReboot (power key handling lives in InputHandler)
**********************************************************************/
void checkReboot() {}

/***************************************************************************************
** Function name: isCharging()
***************************************************************************************/
bool isCharging() { return false; }
