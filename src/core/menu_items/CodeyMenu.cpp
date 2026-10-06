#include "CodeyMenu.h"
#include "codey_hw.h"

/*********************************************************************
** Helpers for the 16x8 matrix (text + bar only, no off-screen chrome)
*********************************************************************/
static void tinyShow(const String &s, bool numbers = false) {
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.setTextFont(numbers ? 2 : 1);
    tft.drawString(s.substring(0, 8), 0, 0);
    tft.setTextFont(1);
    tft.flushNow();
}

static void tinyBarRow(int value) {
    // bottom row progress 0-100 across 16 columns
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    int filled = (value * 16) / 100;
    for (int x = 0; x < 16; x++) {
        tft.drawPixel(x, 7, x < filled ? bruceConfig.priColor : bruceConfig.bgColor);
    }
}

// live sensor: 3x5 number + bottom bar, Esc to exit
static void liveValueView(const char *label, std::function<int()> readFn) {
    unsigned long lastDraw = 0;
    while (1) {
        if (check(EscPress) || returnToMenu) {
            while (check(EscPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            break;
        }
        if (millis() - lastDraw > 120) {
            lastDraw = millis();
            int v = readFn();
            char buf[8];
            // "L99" style fits 3x5
            snprintf(buf, sizeof(buf), "%s%02d", label, v % 100);
            tft.fillScreen(bruceConfig.bgColor);
            tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
            tft.setTextSize(1);
            tft.setTextFont(2);
            tft.drawString(buf, 0, 0);
            tft.setTextFont(1);
            tinyBarRow(v);
            tft.flushNow();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    tft.fillScreen(bruceConfig.bgColor);
    tft.flushNow();
}

/*********************************************************************
** Menu
*********************************************************************/
void CodeyMenu::optionsMenu(void) {
    options = {
        {"Light", [this]() { lightApp(); }},
        {"Sound", [this]() { soundApp(); }},
        {"Knob", [this]() { knobApp(); }},
        {"IMU", [this]() { imuApp(); }},
        {"RGB Led", [this]() { rgbApp(); }},
        {"Speaker", [this]() { speakerApp(); }},
        {"Faces", [this]() { facesApp(); }},
        {"Rocky Base", [this]() { rockyApp(); }},
    };
    delay(200);
    loopOptions(options, MENU_TYPE_SUBMENU, "Sensors");
}

void CodeyMenu::drawIcon(float scale) {
    // Simple face on the 16x8 matrix
    tft.fillScreen(bruceConfig.bgColor);
    tft.drawPixel(4, 2, bruceConfig.priColor);
    tft.drawPixel(11, 2, bruceConfig.priColor);
    tft.drawPixel(4, 5, bruceConfig.priColor);
    tft.drawPixel(5, 6, bruceConfig.priColor);
    tft.drawPixel(6, 6, bruceConfig.priColor);
    tft.drawPixel(7, 6, bruceConfig.priColor);
    tft.drawPixel(8, 6, bruceConfig.priColor);
    tft.drawPixel(9, 6, bruceConfig.priColor);
    tft.drawPixel(10, 6, bruceConfig.priColor);
    tft.drawPixel(11, 5, bruceConfig.priColor);
    tft.flushNow();
}

void CodeyMenu::lightApp(void) { liveValueView("L", CodeyHW::lightSensor); }

void CodeyMenu::soundApp(void) { liveValueView("S", CodeyHW::soundSensor); }

void CodeyMenu::knobApp(void) { liveValueView("K", CodeyHW::knob); }

void CodeyMenu::imuApp(void) {
    tinyShow("CAL");
    if (!CodeyHW::imuBegin()) {
        tinyShow("NOIM");
        delay(800);
        return;
    }
    unsigned long lastDraw = 0;
    while (1) {
        if (check(EscPress) || returnToMenu) {
            while (check(EscPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            break;
        }
        if (millis() - lastDraw > 80) {
            lastDraw = millis();
            float pitch, roll;
            CodeyHW::imuAngles(pitch, roll);
            tft.fillScreen(bruceConfig.bgColor);
            // crosshair center
            tft.drawPixel(7, 3, bruceConfig.priColor);
            tft.drawPixel(8, 3, bruceConfig.priColor);
            tft.drawPixel(7, 4, bruceConfig.priColor);
            tft.drawPixel(8, 4, bruceConfig.priColor);
            // bubble from pitch/roll
            int x = 7 + (int)constrain(roll / 35.0f, -1.0f, 1.0f) * 6;
            int y = 3 + (int)constrain(pitch / 35.0f, -1.0f, 1.0f) * 3;
            if (x < 0) x = 0;
            if (x > 15) x = 15;
            if (y < 0) y = 0;
            if (y > 7) y = 7;
            tft.drawPixel(x, y, bruceConfig.priColor);
            tft.drawPixel(x + 1 > 15 ? x : x + 1, y, bruceConfig.priColor);
            tft.flushNow();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    tft.fillScreen(bruceConfig.bgColor);
    tft.flushNow();
}

void CodeyMenu::rgbApp(void) {
    // cycle preset colors with Next/Prev, Esc exit
    struct { const char *n; uint8_t r, g, b; } cols[] = {
        {"RED", 255, 0, 0}, {"GRN", 0, 255, 0}, {"BLU", 0, 0, 255},
        {"WHT", 255, 255, 255}, {"OFF", 0, 0, 0},
    };
    int i = 0;
    auto show = [&]() {
        CodeyHW::rgbSet(cols[i].r, cols[i].g, cols[i].b);
        tinyShow(cols[i].n);
    };
    show();
    while (1) {
        if (check(EscPress) || returnToMenu) {
            while (check(EscPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            break;
        }
        if (check(NextPress)) {
            i = (i + 1) % 5;
            show();
            while (check(NextPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        if (check(PrevPress)) {
            i = (i + 4) % 5;
            show();
            while (check(PrevPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    CodeyHW::rgbOff();
    tft.fillScreen(bruceConfig.bgColor);
    tft.flushNow();
}

void CodeyMenu::speakerApp(void) {
    tinyShow("BEEP");
    CodeyHW::speakerTone(880, 200);
    delay(50);
    CodeyHW::speakerTone(1320, 200);
    delay(50);
    CodeyHW::speakerTone(1760, 300);
    CodeyHW::speakerNoTone();
    tinyShow("OK");
    delay(400);
}

void CodeyMenu::facesApp(void) {
    int n = CodeyHW::faceCount();
    if (n <= 0) {
        tinyShow("NONE");
        delay(600);
        return;
    }
    int i = 0;
    while (1) {
        if (check(EscPress) || returnToMenu) {
            while (check(EscPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            break;
        }
        if (check(NextPress) || check(SelPress)) {
            i = (i + 1) % n;
            while (check(NextPress) || check(SelPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            CodeyHW::playFace(i);
        }
        if (check(PrevPress)) {
            i = (i + n - 1) % n;
            while (check(PrevPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            CodeyHW::playFace(i);
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    tft.fillScreen(bruceConfig.bgColor);
    tft.flushNow();
}

void CodeyMenu::rockyApp(void) {
    tinyShow("ROCK");
    CodeyHW::rockyBegin();
    // give discovery a moment
    for (int i = 0; i < 20; i++) {
        CodeyHW::rockyTick();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    if (!CodeyHW::rockyFound()) {
        tinyShow("NOBS"); // no base
        delay(800);
        // still offer drive menu in case base appears later
    }
    options = {
        {"Drive", [this]() { rockyDriveMenu(); }},
        {"Status", [this]() { rockyStatusView(); }},
        {"Line", [this]() { rockyLineView(); }},
        {"Stop", []() { CodeyHW::rockyStop(); }},
    };
    loopOptions(options, MENU_TYPE_SUBMENU, "Rocky");
    CodeyHW::rockyStop();
}

void CodeyMenu::rockyDriveMenu(void) {
    int speed = 40;
    tinyShow("DRV");
    while (1) {
        CodeyHW::rockyTick();
        if (check(EscPress) || returnToMenu) {
            while (check(EscPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            break;
        }
        // A = forward, B = left, C = right (Next), long Esc stop already
        if (check(SelPress)) {
            CodeyHW::rockyForward(speed);
            tinyShow("FWD");
            while (check(SelPress)) {
                CodeyHW::rockyTick();
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }
            CodeyHW::rockyStop();
            tinyShow("STOP");
        }
        if (check(PrevPress)) {
            CodeyHW::rockyLeft(speed);
            tinyShow("LFT");
            while (check(PrevPress)) {
                CodeyHW::rockyTick();
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }
            CodeyHW::rockyStop();
            tinyShow("STOP");
        }
        if (check(NextPress)) {
            CodeyHW::rockyRight(speed);
            tinyShow("RGT");
            while (check(NextPress)) {
                CodeyHW::rockyTick();
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }
            CodeyHW::rockyStop();
            tinyShow("STOP");
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    CodeyHW::rockyStop();
}

void CodeyMenu::rockyStatusView(void) {
    unsigned long lastDraw = 0;
    while (1) {
        if (check(EscPress) || returnToMenu) {
            while (check(EscPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            break;
        }
        CodeyHW::rockyTick();
        if (millis() - lastDraw > 150) {
            lastDraw = millis();
            char buf[8];
            if (!CodeyHW::rockyFound()) {
                tinyShow("NOBS");
            } else {
                int g = CodeyHW::rockyGrey();
                int o = CodeyHW::rockyObstacle();
                // Gxx or OBS
                if (o > 0) snprintf(buf, sizeof(buf), "OBS");
                else if (g >= 0) snprintf(buf, sizeof(buf), "G%02d", g % 100);
                else snprintf(buf, sizeof(buf), "OK");
                tft.fillScreen(bruceConfig.bgColor);
                tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
                tft.setTextFont(2);
                tft.drawString(buf, 0, 0);
                tft.setTextFont(1);
                if (g >= 0) tinyBarRow(g);
                tft.flushNow();
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    tft.fillScreen(bruceConfig.bgColor);
    tft.flushNow();
}

void CodeyMenu::rockyLineView(void) {
    unsigned long lastDraw = 0;
    while (1) {
        if (check(EscPress) || returnToMenu) {
            while (check(EscPress)) vTaskDelay(10 / portTICK_PERIOD_MS);
            break;
        }
        CodeyHW::rockyTick();
        if (millis() - lastDraw > 120) {
            lastDraw = millis();
            int l = CodeyHW::rockyReflectL();
            int r = CodeyHW::rockyReflectR();
            if (l < 0) l = 0;
            if (r < 0) r = 0;
            int lh = 1 + (l * 7) / 100;
            int rh = 1 + (r * 7) / 100;
            tft.fillScreen(bruceConfig.bgColor);
            for (int y = 0; y < lh; y++)
                for (int x = 0; x < 7; x++) tft.drawPixel(x, 7 - y, bruceConfig.priColor);
            for (int y = 0; y < rh; y++)
                for (int x = 9; x < 16; x++) tft.drawPixel(x, 7 - y, bruceConfig.priColor);
            tft.drawPixel(7, 0, bruceConfig.priColor);
            tft.drawPixel(8, 0, bruceConfig.priColor);
            tft.flushNow();
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    tft.fillScreen(bruceConfig.bgColor);
    tft.flushNow();
}
