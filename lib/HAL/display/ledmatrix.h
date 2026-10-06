#ifndef LIB_HAL_LEDMATRIX_H
#define LIB_HAL_LEDMATRIX_H
#include <pins_arduino.h>
#ifdef USE_LEDMATRIX

#include <Arduino.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "tft_defines.h"

/*********************************************************************
** 16x8 monochrome LED matrix backend for Bruce (Makeblock Codey Rocky)
**
** The panel is driven by a TM1640-class controller bit-banged on two
** GPIOs (protocol identical to Makeblock's MicroPython driver).
** Everything Bruce draws is mapped to 1 bit: color != 0 -> LED on.
** Text uses an embedded 3x5 font (4px advance -> 4 chars per line).
*********************************************************************/

#define LM_W 16
#define LM_H 8

// Font metrics expected by display.cpp (scrolling, layout helpers).
// Size 1 = 3x5 glyph / 4 px advance; size >= 2 = authentic 6x8.
#ifndef LW
#define LW 4
#endif
#ifndef LH
#define LH 8
#endif

// 3x5 font, glyph = 15 bits: col-major, 5 bits per column, bit r = row r (top)
extern const uint16_t lm_font3x5[95];


// Low level matrix driver
class LedMatrix {
public:
    static void begin();
    static void setBrightness(uint8_t level); // 1..8, 0 = off
    static void pushFrame(const uint8_t *fb); // fb: 16 bytes, bit y = row y
    static void hardwareOff();
};

class tft_display;
class tft_logger;
class tft_sprite;

class tft_display {
public:
    explicit tft_display(int16_t _W = TFT_WIDTH, int16_t _H = TFT_HEIGHT);
    friend class tft_sprite;
    friend class tft_logger;

    void begin(uint32_t speed = 0);
    void init(uint8_t tc = 0);
    void setRotation(uint8_t r);
    void invertDisplay(bool i);
    void sleep(bool value);

    void drawPixel(int32_t x, int32_t y, uint32_t color);
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color);
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color);
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void fillRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2);
    void fillRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2);
    void fillScreen(uint32_t color);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
    void drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color);
    void fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color);
    void drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void drawEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color);
    void fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color);
    void drawArc(
        int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t startAngle, uint32_t endAngle,
        uint32_t fg_color, uint32_t bg_color, bool smoothArc = true
    );
    void drawWideLine(
        float ax, float ay, float bx, float by, float wd, uint32_t fg_color, uint32_t bg_color = 0x00FFFFFF
    );
    void drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
    void drawXBitmap(
        int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg
    );
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap);
    void
    pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, uint16_t *cmap);

    void setSwapBytes(bool swap);
    bool getSwapBytes() const;
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const;

    int16_t textWidth(const String &s, uint8_t font = 1) const;
    int16_t textWidth(const char *s, uint8_t font = 1) const;
    int16_t fontHeight(int16_t font = 1) const;

    void setCursor(int16_t x, int16_t y);
    int16_t getCursorX() const;
    int16_t getCursorY() const;
    void setTextSize(uint8_t s);
    void setTextColor(uint16_t c);
    void setTextColor(uint16_t c, uint16_t b, bool bgfill = false);
    void setTextDatum(uint8_t d);
    uint8_t getTextDatum() const;
    void setTextFont(uint8_t f);
    void setTextWrap(bool wrapX, bool wrapY = false);
    int16_t drawString(const String &string, int32_t x, int32_t y, uint8_t font = 1);
    int16_t drawCentreString(const String &string, int32_t x, int32_t y, uint8_t font = 1);
    int16_t drawRightString(const String &string, int32_t x, int32_t y, uint8_t font = 1);

    size_t write(uint8_t c);
    size_t write(const uint8_t *buffer, size_t size);
    template <typename T> size_t print(const T &val) {
        String s = String(val);
        return write((const uint8_t *)s.c_str(), s.length());
    }
    template <typename T> size_t println(const T &val) {
        size_t n = print(val);
        write('\n');
        return n + 1;
    }
    size_t println();
    size_t printf(const char *fmt, ...);

    int16_t width() const;
    int16_t height() const;
    uint8_t getRotation() const;
    SPIClass &getSPIinstance() const;
    void writecommand(uint8_t c);

    uint32_t getTextColor() const;
    uint32_t getTextBgColor() const;
    uint8_t getTextSize() const;

    // direct matrix helpers used by the TINY_DISPLAY UI
    void invertRect(int32_t x, int32_t y, int32_t w, int32_t h);
    // Push the current framebuffer to the panel as ONE complete frame,
    // synchronously - same semantics as the tested driver's show().
    // Skipped while a composite draw is in progress (_suppress).
    void flushNow();

private:
    bool _suppress = false; // true while a composite draw op is building a frame

    void setPixelRaw(int32_t x, int32_t y, bool on);
    void fillRectRaw(int32_t x, int32_t y, int32_t w, int32_t h, bool on);

    uint8_t fb[LM_W];  // framebuffer, bit y = row y
    bool dirty = true; // needs a push to the panel

    int16_t _width = LM_W;
    int16_t _height = LM_H;
    int16_t _cursorX = 0;
    int16_t _cursorY = 0;
    uint8_t _textSize = 1;
    uint8_t _textFont = 1;
    uint32_t _textColor = 1;
    uint32_t _textBgColor = 0;
    uint8_t _textDatum = 0;
    uint8_t _rotation = 0;
    bool _swapBytes = false;
    bool _asleep = false;
    SemaphoreHandle_t _mutex = nullptr;
};

class tft_sprite {
public:
    explicit tft_sprite(tft_display *parent);
    ~tft_sprite() = default;

    void *createSprite(int16_t w, int16_t h, uint8_t frames = 1);
    void deleteSprite();
    void setColorDepth(uint8_t depth);

    void drawPixel(int32_t x, int32_t y, uint32_t color);
    void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
    void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color);
    void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color);
    void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void fillScreen(uint32_t color);
    void drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
    void fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
    void drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color);
    void fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color);
    void fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color);
    void drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
    void drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);

    void setCursor(int16_t x, int16_t y);
    void setTextColor(uint16_t c);
    void setTextColor(uint16_t c, uint16_t b, bool bgfill = false);
    void setTextSize(uint8_t s);
    void setTextDatum(uint8_t d);
    int16_t drawString(const String &string, int32_t x, int32_t y, uint8_t font = 1);
    int16_t drawCentreString(const String &string, int32_t x, int32_t y, uint8_t font = 1);
    size_t print(const String &s);
    size_t println(const String &s);

    void pushSprite(int32_t x, int32_t y, uint32_t transparent = TFT_TRANSPARENT);
    void pushToSprite(tft_sprite *dest, int32_t x, int32_t y, uint32_t transparent = TFT_TRANSPARENT);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data);
    void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap);
    void
    pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, uint16_t *cmap);

    void fillRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2);
    void fillRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2);

    int16_t width() const;
    int16_t height() const;

private:
    tft_display *_parent;
    uint16_t *buf = nullptr;
    int16_t _w = 0;
    int16_t _h = 0;
    int16_t _cursorX = 0;
    int16_t _cursorY = 0;
    uint8_t _textSize = 1;
    uint32_t _textColor = 1;
    uint32_t _textBgColor = 0;
    uint8_t _textDatum = 0;
};

#endif // USE_LEDMATRIX
#endif // LIB_HAL_LEDMATRIX_H
