#include "ledmatrix.h"
#ifdef USE_LEDMATRIX

#include "codey_fonts.h" // authentic Makeblock fonts + animations (hardware-tested)

/*********************************************************************
** Low level: TM1640-class LED matrix controller, bit-banged.
** Protocol copied from Makeblock's MicroPython port (codey_ledmatrix.c)
** - command 0x40: auto address increment mode
** - 0xC0|addr  : set address, followed by data bytes (LSB first)
** - 0x88..0x8F : display on + brightness, 0x80: display off
*********************************************************************/

// tunables, same defaults as the hardware-tested codey-sniffer project
#ifndef MATRIX_BIT_DELAY_US
#define MATRIX_BIT_DELAY_US 3
#endif
#ifndef MATRIX_FLIP_X
#define MATRIX_FLIP_X 0
#endif
#ifndef MATRIX_FLIP_Y
#define MATRIX_FLIP_Y 0
#endif

static const int LM_SCL = CODEY_MATRIX_SCL;
static const int LM_SDA = CODEY_MATRIX_SDA;

// Frame sequence copied verbatim from the tested CodeyMatrix driver:
//   line HIGH/HIGH -> cmd 0x40 (start+bits+stop) -> start -> 0xC0|0
//   -> 16 data bytes (LSB first) -> stop.
// NOTE: the 0xC0 address byte and the data MUST share one transmission
// (no stop in between) or the panel stays blank.
static void lm_line(int pin, int level) {
    digitalWrite(pin, level);
    delayMicroseconds(MATRIX_BIT_DELAY_US);
}

static void lm_start() {
    lm_line(LM_SCL, HIGH);
    lm_line(LM_SDA, HIGH);
    lm_line(LM_SDA, LOW);
}

static void lm_stop() {
    lm_line(LM_SCL, LOW);
    lm_line(LM_SDA, LOW);
    lm_line(LM_SCL, HIGH);
    lm_line(LM_SDA, HIGH);
}

static void lm_bits(uint8_t d) {
    for (uint8_t i = 0; i < 8; i++) {
        lm_line(LM_SCL, LOW);
        lm_line(LM_SDA, d & 0x01);
        lm_line(LM_SCL, HIGH);
        d >>= 1;
    }
}

static void lm_command(uint8_t cmd) {
    lm_start();
    lm_bits(cmd);
    lm_stop();
}

static void lm_write_frame(const uint8_t *data16) {
    lm_line(LM_SCL, HIGH);
    lm_line(LM_SDA, HIGH);
    lm_command(0x40);       // auto-increment address mode
    lm_start();
    lm_bits(0xC0 | 0);      // address 0
    for (int i = 0; i < 16; i++) lm_bits(data16[i]);
    lm_stop();
}

static uint8_t lm_rev8(uint8_t d) {
    d = (uint8_t)(((d & 0xF0) >> 4) | ((d & 0x0F) << 4));
    d = (uint8_t)(((d & 0xCC) >> 2) | ((d & 0x33) << 2));
    d = (uint8_t)(((d & 0xAA) >> 1) | ((d & 0x55) << 1));
    return d;
}

void LedMatrix::begin() {
    pinMode(LM_SCL, OUTPUT);
    pinMode(LM_SDA, OUTPUT);
    lm_line(LM_SCL, HIGH);
    lm_line(LM_SDA, HIGH);
    lm_command(0x40);
    setBrightness(8);
    uint8_t zeros[16] = {0};
    pushFrame(zeros);
}

void LedMatrix::setBrightness(uint8_t level) {
    if (level > 8) level = 8;
    uint8_t code = (level == 0) ? 0x80 : (0x80 | ((level - 1) | 0x08));
    lm_command(code);
}

// Orientation fixes (LM_FLIP_X / LM_FLIP_Y) applied at push time, exactly
// like the tested driver does in show().
void LedMatrix::pushFrame(const uint8_t *fb) {
    uint8_t out[16];
    for (int x = 0; x < 16; x++) {
        uint8_t v = MATRIX_FLIP_X ? fb[15 - x] : fb[x];
        out[x] = MATRIX_FLIP_Y ? lm_rev8(v) : v;
    }
    lm_write_frame(out);
}

void LedMatrix::hardwareOff() {
    setBrightness(0);
    uint8_t zeros[16] = {0};
    pushFrame(zeros);
}

/*********************************************************************
** 3x5 font — redesigned for legibility on a 16x8 LED matrix.
** Encoding: each glyph = 3 columns × 5 rows.
**   bit 0 = top row, bit 4 = bottom row of a column.
**   G(c0,c1,c2) packs the three column bitmasks.
** Lowercase maps to the same uppercase shapes.
*********************************************************************/
// each G(a,b,c) packs one glyph: a/b/c = 5-bit patterns of column 0/1/2
#define G(c0, c1, c2) ((uint16_t)((c0) | ((c1) << 5) | ((c2) << 10)))
const uint16_t lm_font3x5[95] = {
    /* 0x20 ' ' */ 0x0000,
    /* 0x21 '!' */ G(0x00, 0x17, 0x00),
    /* 0x22 '"' */ G(0x03, 0x00, 0x03),
    /* 0x23 '#' */ G(0x0A, 0x1F, 0x0A),
    /* 0x24 '$' */ G(0x12, 0x1F, 0x09),
    /* 0x25 '%' */ G(0x19, 0x04, 0x13),
    /* 0x26 '&' */ G(0x1A, 0x15, 0x1A),
    /* 0x27 '\''*/ G(0x00, 0x03, 0x00),
    /* 0x28 '(' */ G(0x00, 0x0E, 0x11),
    /* 0x29 ')' */ G(0x11, 0x0E, 0x00),
    /* 0x2A '*' */ G(0x04, 0x0E, 0x04),
    /* 0x2B '+' */ G(0x04, 0x0E, 0x04),
    /* 0x2C ',' */ G(0x10, 0x08, 0x00),
    /* 0x2D '-' */ G(0x04, 0x04, 0x04),
    /* 0x2E '.' */ G(0x00, 0x10, 0x00),
    /* 0x2F '/' */ G(0x18, 0x04, 0x03),

    /* DIGITS */
    /* 0x30 '0' */ G(0x1F, 0x11, 0x1F),
    /* 0x31 '1' */ G(0x00, 0x1F, 0x00),
    /* 0x32 '2' */ G(0x1D, 0x15, 0x17),
    /* 0x33 '3' */ G(0x15, 0x15, 0x1F),
    /* 0x34 '4' */ G(0x07, 0x04, 0x1F),
    /* 0x35 '5' */ G(0x17, 0x15, 0x1D),
    /* 0x36 '6' */ G(0x1F, 0x15, 0x1D),
    /* 0x37 '7' */ G(0x01, 0x01, 0x1F),
    /* 0x38 '8' */ G(0x1F, 0x15, 0x1F),
    /* 0x39 '9' */ G(0x17, 0x15, 0x1F),

    /* 0x3A ':' */ G(0x00, 0x0A, 0x00),
    /* 0x3B ';' */ G(0x10, 0x0A, 0x00),
    /* 0x3C '<' */ G(0x04, 0x0A, 0x11),
    /* 0x3D '=' */ G(0x0A, 0x0A, 0x0A),
    /* 0x3E '>' */ G(0x11, 0x0A, 0x04),
    /* 0x3F '?' */ G(0x01, 0x15, 0x02),
    /* 0x40 '@' */ G(0x1F, 0x11, 0x17),

    /* A–Z */
    /* 0x41 'A' */ G(0x1E, 0x05, 0x1E),
    /* 0x42 'B' */ G(0x1F, 0x15, 0x0A),
    /* 0x43 'C' */ G(0x0E, 0x11, 0x11),
    /* 0x44 'D' */ G(0x1F, 0x11, 0x0E),
    /* 0x45 'E' */ G(0x1F, 0x15, 0x15),
    /* 0x46 'F' */ G(0x1F, 0x05, 0x05),
    /* 0x47 'G' */ G(0x0E, 0x11, 0x19),
    /* 0x48 'H' */ G(0x1F, 0x04, 0x1F),
    /* 0x49 'I' */ G(0x11, 0x1F, 0x11),
    /* 0x4A 'J' */ G(0x08, 0x10, 0x0F),
    /* 0x4B 'K' */ G(0x1F, 0x04, 0x1B),
    /* 0x4C 'L' */ G(0x1F, 0x10, 0x10),
    /* 0x4D 'M' */ G(0x1F, 0x02, 0x1F),
    /* 0x4E 'N' */ G(0x1F, 0x01, 0x1F),
    /* 0x4F 'O' */ G(0x0E, 0x11, 0x0E),
    /* 0x50 'P' */ G(0x1F, 0x05, 0x07),
    /* 0x51 'Q' */ G(0x0E, 0x19, 0x1E),
    /* 0x52 'R' */ G(0x1F, 0x05, 0x1B),
    /* 0x53 'S' */ G(0x12, 0x15, 0x09),
    /* 0x54 'T' */ G(0x01, 0x1F, 0x01),
    /* 0x55 'U' */ G(0x0F, 0x10, 0x0F),
    /* 0x56 'V' */ G(0x07, 0x18, 0x07),
    /* 0x57 'W' */ G(0x1F, 0x08, 0x1F),
    /* 0x58 'X' */ G(0x1B, 0x04, 0x1B),
    /* 0x59 'Y' */ G(0x03, 0x1C, 0x03),
    /* 0x5A 'Z' */ G(0x19, 0x15, 0x13),

    /* 0x5B '[' */ G(0x1F, 0x11, 0x00),
    /* 0x5C '\\'*/ G(0x03, 0x04, 0x18),
    /* 0x5D ']' */ G(0x00, 0x11, 0x1F),
    /* 0x5E '^' */ G(0x02, 0x01, 0x02),
    /* 0x5F '_' */ G(0x10, 0x10, 0x10),
    /* 0x60 '`' */ G(0x01, 0x02, 0x00),

    /* a-z = A-Z */
    /* a */ G(0x1E, 0x05, 0x1E),
    /* b */ G(0x1F, 0x15, 0x0A),
    /* c */ G(0x0E, 0x11, 0x11),
    /* d */ G(0x1F, 0x11, 0x0E),
    /* e */ G(0x1F, 0x15, 0x15),
    /* f */ G(0x1F, 0x05, 0x05),
    /* g */ G(0x0E, 0x11, 0x19),
    /* h */ G(0x1F, 0x04, 0x1F),
    /* i */ G(0x11, 0x1F, 0x11),
    /* j */ G(0x08, 0x10, 0x0F),
    /* k */ G(0x1F, 0x04, 0x1B),
    /* l */ G(0x1F, 0x10, 0x10),
    /* m */ G(0x1F, 0x02, 0x1F),
    /* n */ G(0x1F, 0x01, 0x1F),
    /* o */ G(0x0E, 0x11, 0x0E),
    /* p */ G(0x1F, 0x05, 0x07),
    /* q */ G(0x0E, 0x19, 0x1E),
    /* r */ G(0x1F, 0x05, 0x1B),
    /* s */ G(0x12, 0x15, 0x09),
    /* t */ G(0x01, 0x1F, 0x01),
    /* u */ G(0x0F, 0x10, 0x0F),
    /* v */ G(0x07, 0x18, 0x07),
    /* w */ G(0x1F, 0x08, 0x1F),
    /* x */ G(0x1B, 0x04, 0x1B),
    /* y */ G(0x03, 0x1C, 0x03),
    /* z */ G(0x19, 0x15, 0x13),

    /* 0x7B '{' */ G(0x04, 0x1B, 0x11),
    /* 0x7C '|' */ G(0x00, 0x1F, 0x00),
    /* 0x7D '}' */ G(0x11, 0x1B, 0x04),
    /* 0x7E '~' */ G(0x04, 0x02, 0x04),
};
#undef G

/*********************************************************************
** tft_display
*********************************************************************/
static tft_display *g_matrix_display = nullptr;

tft_display::tft_display(int16_t _W, int16_t _H) {
    _width = LM_W;
    _height = LM_H;
    memset(fb, 0, sizeof(fb));
    g_matrix_display = this;
}

void tft_display::begin(uint32_t speed) { init(); }

void tft_display::init(uint8_t tc) {
    if (_mutex == nullptr) _mutex = xSemaphoreCreateMutex();
    LedMatrix::begin();
    memset(fb, 0, sizeof(fb));
    dirty = true;
    flushNow();
}

// One complete, synchronous frame push - this mirrors the tested driver,
// where every finished drawing step lands on the panel immediately.
// _suppress keeps composite operations (circles, strings, sprites...) from
// pushing partial frames: only their final result hits the matrix.
void tft_display::flushNow() {
    if (_suppress) return;
    uint8_t copy[LM_W];
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    memcpy(copy, fb, LM_W);
    dirty = false;
    if (_mutex) xSemaphoreGive(_mutex);
    LedMatrix::pushFrame(copy);
}

void tft_display::setPixelRaw(int32_t x, int32_t y, bool on) {
    if (x < 0 || x >= LM_W || y < 0 || y >= LM_H) return;
    if (on) fb[x] |= (1 << y);
    else fb[x] &= ~(1 << y);
}

void tft_display::fillRectRaw(int32_t x, int32_t y, int32_t w, int32_t h, bool on) {
    for (int32_t j = y; j < y + h; j++) {
        if (j < 0 || j >= LM_H) continue;
        for (int32_t i = x; i < x + w; i++) {
            if (i < 0 || i >= LM_W) continue;
            if (on) fb[i] |= (1 << j);
            else fb[i] &= ~(1 << j);
        }
    }
}

void tft_display::drawPixel(int32_t x, int32_t y, uint32_t color) {
    // Never push a full frame per pixel: the TM1640 bit-bang is ~1-2 ms
    // per frame. Mark dirty and let high-level ops (drawString, fillRect,
    // flushNow) push once when the composite is complete.
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    setPixelRaw(x, y, color != 0);
    dirty = true;
    if (_mutex) xSemaphoreGive(_mutex);
    // Only auto-flush when not inside a composite draw and the caller is
    // a one-off pixel (rare). Prefer explicit flushNow() from UI code.
    if (!_suppress) {
        // batch: do not flush here; caller must call flushNow()
    }
}

void tft_display::fillScreen(uint32_t color) {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    memset(fb, color != 0 ? 0xFF : 0x00, sizeof(fb));
    dirty = true;
    if (_mutex) xSemaphoreGive(_mutex);
    flushNow();
}

void tft_display::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    bool on = color != 0;
    int32_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int32_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    while (true) {
        setPixelRaw(x0, y0, on);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
    dirty = true;
    if (_mutex) xSemaphoreGive(_mutex);
    flushNow();
}

void tft_display::drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
    fillRect(x, y, w, 1, color);
}

void tft_display::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    fillRect(x, y, 1, h, color);
}

void tft_display::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    fillRectRaw(x, y, w, h, color != 0);
    dirty = true;
    if (_mutex) xSemaphoreGive(_mutex);
    flushNow();
}

void tft_display::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    _suppress = true;
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
    _suppress = false;
    flushNow();
}

void tft_display::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    drawRect(x, y, w, h, color);
}

void tft_display::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    fillRect(x, y, w, h, color);
}

void tft_display::drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    _suppress = true;
    int32_t f = 1 - r, ddx = 1, ddy = -2 * r, x = 0, y = r;
    while (x < y) {
        if (f >= 0) {
            y--;
            ddy += 2;
            f += ddy;
        }
        x++;
        ddx += 2;
        f += ddx;
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 + x, y0 - y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 - y, y0 - x, color);
    }
    _suppress = false;
    flushNow();
}

void tft_display::fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    _suppress = true;
    for (int32_t y = -r; y <= r; y++)
        for (int32_t x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) drawPixel(x0 + x, y0 + y, color);
    _suppress = false;
    flushNow();
}

void tft_display::drawTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    _suppress = true;
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
    _suppress = false;
    flushNow();
}

void tft_display::fillTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    // flat-foot fill via scanline, fine for the tiny panel
    _suppress = true;
    int32_t miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int32_t maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    for (int32_t y = miny; y <= maxy; y++) {
        int32_t xmin = INT32_MAX, xmax = INT32_MIN;
        int32_t xa = x0, xb = x1, xc = x2;
        int32_t ya = y0, yb = y1, yc = y2;
        int32_t xs[3] = {xa, xb, xc}, ys[3] = {ya, yb, yc};
        for (int e = 0; e < 3; e++) {
            int32_t xA = xs[e], yA = ys[e], xB = xs[(e + 1) % 3], yB = ys[(e + 1) % 3];
            if ((yA <= y && y < yB) || (yB <= y && y < yA)) {
                int32_t x = xA + (int32_t)((float)(y - yA) * (xB - xA) / (float)(yB - yA));
                if (x < xmin) xmin = x;
                if (x > xmax) xmax = x;
            }
        }
        if (xmax >= xmin) drawFastHLine(xmin, y, xmax - xmin + 1, color);
    }
    _suppress = false;
    flushNow();
}

void tft_display::drawEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    if (rx < 1 || ry < 1) return;
    _suppress = true;
    for (int32_t a = 0; a < 360; a += 15) {
        int32_t x = x0 + (int32_t)(rx * cosf(a * 0.0174533f));
        int32_t y = y0 + (int32_t)(ry * sinf(a * 0.0174533f));
        drawPixel(x, y, color);
    }
    _suppress = false;
    flushNow();
}

void tft_display::fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    if (ry < 1) return;
    _suppress = true;
    for (int32_t y = -ry; y <= ry; y++) {
        int32_t w = (int32_t)(rx * sqrtf(1.0f - (float)y * y / (float)(ry * ry)));
        drawFastHLine(x0 - w, y0 + y, 2 * w + 1, color);
    }
    _suppress = false;
    flushNow();
}

void tft_display::drawArc(
    int32_t x, int32_t y, int32_t r, int32_t ir, uint32_t startAngle, uint32_t endAngle,
    uint32_t fg_color, uint32_t bg_color, bool smoothArc
) {
    if (endAngle < startAngle) endAngle += 360;
    _suppress = true;
    for (uint32_t a = startAngle; a < endAngle; a += 10) {
        float rad = a * 0.0174533f;
        for (int32_t rr = ir; rr <= r; rr++) {
            int32_t px = x + (int32_t)(rr * cosf(rad));
            int32_t py = y - (int32_t)(rr * sinf(rad));
            drawPixel(px, py, fg_color);
        }
    }
    _suppress = false;
    flushNow();
}

void tft_display::drawWideLine(
    float ax, float ay, float bx, float by, float wd, uint32_t fg_color, uint32_t bg_color
) {
    drawLine((int32_t)ax, (int32_t)ay, (int32_t)bx, (int32_t)by, fg_color);
}

void tft_display::drawXBitmap(
    int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color
) {
    _suppress = true;
    int32_t byteWidth = (w + 7) / 8;
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            if (bitmap[j * byteWidth + i / 8] & (1 << (i % 8))) drawPixel(x + i, y + j, color);
        }
    }
    _suppress = false;
    flushNow();
}

void tft_display::drawXBitmap(
    int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg
) {
    _suppress = true;
    int32_t byteWidth = (w + 7) / 8;
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            bool on = bitmap[j * byteWidth + i / 8] & (1 << (i % 8));
            drawPixel(x + i, y + j, on ? color : bg);
        }
    }
    _suppress = false;
    flushNow();
}

void tft_display::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data) {
    _suppress = true;
    for (int32_t j = 0; j < h; j++)
        for (int32_t i = 0; i < w; i++) drawPixel(x + i, y + j, data[j * w + i]);
    _suppress = false;
    flushNow();
}

void tft_display::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *data) {
    pushImage(x, y, w, h, (const uint16_t *)data);
}

void tft_display::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap
) {
    _suppress = true;
    for (int32_t j = 0; j < h; j++)
        for (int32_t i = 0; i < w; i++) {
            uint8_t idx = data[j * w + i];
            drawPixel(x + i, y + j, cmap ? cmap[idx] : idx);
        }
    _suppress = false;
    flushNow();
}

void tft_display::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, uint16_t *cmap
) {
    pushImage(x, y, w, h, (uint8_t *)data, bpp8, cmap);
}

void tft_display::invertRect(int32_t x, int32_t y, int32_t w, int32_t h) {
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    for (int32_t j = y; j < y + h; j++) {
        if (j < 0 || j >= LM_H) continue;
        for (int32_t i = x; i < x + w; i++) {
            if (i < 0 || i >= LM_W) continue;
            fb[i] ^= (1 << j);
        }
    }
    dirty = true;
    if (_mutex) xSemaphoreGive(_mutex);
    flushNow();
}

void tft_display::setSwapBytes(bool swap) { _swapBytes = swap; }
bool tft_display::getSwapBytes() const { return _swapBytes; }
uint16_t tft_display::color565(uint8_t r, uint8_t g, uint8_t b) const {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
void tft_display::setRotation(uint8_t r) { _rotation = r; }
uint8_t tft_display::getRotation() const { return _rotation; }
void tft_display::invertDisplay(bool i) {}
void tft_display::sleep(bool value) {
    _asleep = value;
    LedMatrix::setBrightness(value ? 0 : 8);
}
int16_t tft_display::width() const { return _width; }
int16_t tft_display::height() const { return _height; }
SPIClass &tft_display::getSPIinstance() const {
    static SPIClass dummy;
    return dummy;
}
void tft_display::writecommand(uint8_t c) {}

void tft_display::setCursor(int16_t x, int16_t y) {
    _cursorX = x;
    _cursorY = y;
}
int16_t tft_display::getCursorX() const { return _cursorX; }
int16_t tft_display::getCursorY() const { return _cursorY; }
void tft_display::setTextSize(uint8_t s) {
    if (s < 1) s = 1;
    _textSize = s;
}
void tft_display::setTextColor(uint16_t c) { setTextColor(c, _textBgColor); }
void tft_display::setTextColor(uint16_t c, uint16_t b, bool bgfill) {
    _textColor = c;
    _textBgColor = b;
}
void tft_display::setTextDatum(uint8_t d) { _textDatum = d; }
uint8_t tft_display::getTextDatum() const { return _textDatum; }
void tft_display::setTextFont(uint8_t f) { _textFont = (f == 0) ? 1 : f; }
void tft_display::setTextWrap(bool wrapX, bool wrapY) {}
uint32_t tft_display::getTextColor() const { return _textColor; }
uint32_t tft_display::getTextBgColor() const { return _textBgColor; }
uint8_t tft_display::getTextSize() const { return _textSize; }

int16_t tft_display::textWidth(const String &s, uint8_t font) const {
    // Approximate: trimmed authentic glyphs average ~4 px + gap
    return (int16_t)(s.length() * 4);
}
int16_t tft_display::textWidth(const char *s, uint8_t font) const {
    return (int16_t)(strlen(s) * 4);
}
int16_t tft_display::fontHeight(int16_t font) const {
    return 8; // authentic CHAR_FONT / NUMBER_FONT height
}

// Authentic Makeblock 6x8 CHAR_FONT (codey_ledmatrix.c 2018).
// Font bytes are in "software" orientation → rev8 before display.
// Leading/trailing empty columns are trimmed so letters sit closer together
// and more characters fit on the 16-wide panel.
static int16_t lm_draw_glyph6x8(tft_display *d, uint8_t c, int16_t x, int16_t y, uint32_t color, uint32_t bg) {
    // Always use CHAR_FONT (same bit orientation for digits and letters).
    // NUMBER_FONT used a different packing and appeared vertically flipped.
    const CharFont *f = nullptr;
    for (int i = 0; i < CHAR_FONT_LEN; i++) {
        if (CHAR_FONT[i].c == (char)c) { f = &CHAR_FONT[i]; break; }
    }
    // case-insensitive fallback
    if (!f) {
        char alt = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        for (int i = 0; i < CHAR_FONT_LEN; i++) {
            if (CHAR_FONT[i].c == alt) { f = &CHAR_FONT[i]; break; }
        }
    }
    if (!f) f = &CHAR_FONT[0]; // space

    // find first/last non-empty column
    int first = 0, last = 5;
    while (first < 5 && f->d[first] == 0) first++;
    while (last > first && f->d[last] == 0) last--;

    int out = 0;
    for (int col = first; col <= last; col++) {
        uint8_t bits = lm_rev8(f->d[col]);
        for (uint8_t row = 0; row < 8; row++) {
            d->drawPixel(x + out, y + row, (bits >> row) & 1 ? color : bg);
        }
        out++;
    }
    return out + 1; // glyph width + 1 px gap
}

// renders one glyph of the 3x5 font at (x, y); returns columns advanced
static int16_t lm_draw_glyph(tft_display *d, uint8_t c, int16_t x, int16_t y, uint8_t size, uint32_t color, uint32_t bg) {
    if (c < 32 || c > 126) c = ' ';
    uint16_t bits = lm_font3x5[c - 32];
    for (uint8_t col = 0; col < 3; col++) {
        uint8_t pattern = (bits >> (col * 5)) & 0x1F;
        for (uint8_t row = 0; row < 5; row++) {
            bool on = pattern & (1 << row);
            for (uint8_t sy = 0; sy < size; sy++) {
                for (uint8_t sx = 0; sx < size; sx++) {
                    d->drawPixel(x + col * size + sx, y + row * size + sy, on ? color : bg);
                }
            }
        }
    }
    return 3 * size + 1; // 3 columns + 1 spacing
}

int16_t tft_display::drawString(const String &string, int32_t x, int32_t y, uint8_t font) {
    // font arg or _textFont == 2 → compact 3x5 (clock/timer only)
    const bool use3x5 = (font == 2) || (_textFont == 2);

    int n = 0;
    char chars[16];
    for (size_t i = 0; i < string.length() && n < 16; i++) {
        char c = string[i];
        if (c == '\n') break;
        chars[n++] = c;
    }
    if (n == 0) {
        flushNow();
        return 0;
    }

    if (use3x5) {
        // 3x5 digits: 3 px glyph + 1 px gap = 4 → 4 chars fill 16
        int16_t cx = x;
        // vertically center 5 rows in 8: y+1
        int16_t gy = y + 1;
        _suppress = true;
        for (int i = 0; i < n; i++) {
            char c = chars[i];
            if (c == ' ') {
                cx += 4;
                continue;
            }
            if (c == ':') {
                // two-dot colon in 2 columns
                drawPixel(cx, gy + 1, _textColor);
                drawPixel(cx, gy + 3, _textColor);
                cx += 2;
                continue;
            }
            cx += lm_draw_glyph(this, (uint8_t)c, cx, gy, 1, _textColor, _textBgColor);
        }
        _suppress = false;
        flushNow();
        return cx - x;
    }

    // ---- CHAR_FONT 6x8 path (menus etc.) ----
    int widths[16];
    int content = 0;
    for (int i = 0; i < n; i++) {
        char c = chars[i];
        if (c == ' ') {
            widths[i] = 2;
        } else {
            const CharFont *f = nullptr;
            for (int j = 0; j < CHAR_FONT_LEN; j++) {
                if (CHAR_FONT[j].c == c) { f = &CHAR_FONT[j]; break; }
            }
            if (!f) {
                char alt = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
                for (int j = 0; j < CHAR_FONT_LEN; j++) {
                    if (CHAR_FONT[j].c == alt) { f = &CHAR_FONT[j]; break; }
                }
            }
            if (!f) {
                widths[i] = 4;
            } else {
                int first = 0, last = 5;
                while (first < 5 && f->d[first] == 0) first++;
                while (last > first && f->d[last] == 0) last--;
                widths[i] = last - first + 1;
            }
        }
        content += widths[i];
    }

    const int avail = LM_W - (int)x;
    const int gaps = (n > 1) ? (n - 1) : 0;
    int gapSize = 1;
    int lead = 0;
    if (gaps > 0) {
        int minTotal = content + gaps;
        if (minTotal < avail) {
            int extra = avail - content;
            gapSize = extra / gaps;
            if (gapSize < 1) gapSize = 1;
            lead = (avail - content - gapSize * gaps) / 2;
            if (lead < 0) lead = 0;
        }
    } else if (content < avail) {
        lead = (avail - content) / 2;
    }

    int16_t cx = x + lead;
    _suppress = true;
    for (int i = 0; i < n; i++) {
        if (chars[i] == ' ') {
            cx += widths[i] + (i < n - 1 ? gapSize : 0);
            continue;
        }
        lm_draw_glyph6x8(this, (uint8_t)chars[i], cx, y, _textColor, _textBgColor);
        cx += widths[i];
        if (i < n - 1) cx += gapSize;
    }
    _suppress = false;
    flushNow();
    return cx - x;
}

int16_t tft_display::drawCentreString(const String &string, int32_t x, int32_t y, uint8_t font) {
    // measure with a dry-run on a dummy position then center
    int16_t w = textWidth(string);
    return drawString(string, x - w / 2, y, font);
}

int16_t tft_display::drawRightString(const String &string, int32_t x, int32_t y, uint8_t font) {
    int16_t w = textWidth(string);
    return drawString(string, x - w, y, font);
}

size_t tft_display::write(uint8_t c) {
    if (c == '\n') {
        _cursorX = 0;
        _cursorY += 8;
        if (_cursorY > LM_H) _cursorY = 0;
        return 1;
    }
    if (c == '\r') return 1;
    if (c == ' ') {
        _cursorX += 3;
        if (_cursorX > LM_W) {
            _cursorX = 0;
            _cursorY += 8;
        }
        return 1;
    }
    _cursorX += lm_draw_glyph6x8(this, c, _cursorX, _cursorY, _textColor, _textBgColor);
    if (_cursorX > LM_W) {
        _cursorX = 0;
        _cursorY += 8;
        if (_cursorY > LM_H) _cursorY = 0;
    }
    flushNow();
    return 1;
}

size_t tft_display::write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) n += write(*buffer++);
    return n;
}

size_t tft_display::println() { return write((const uint8_t *)"\n", 1); }

size_t tft_display::printf(const char *fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return write((const uint8_t *)buf, strlen(buf));
}

/*********************************************************************
** tft_sprite: RAM backed 16bpp buffer
*********************************************************************/
tft_sprite::tft_sprite(tft_display *parent) : _parent(parent) {}

void *tft_sprite::createSprite(int16_t w, int16_t h, uint8_t frames) {
    if (buf) free(buf);
    if (w < 1 || h < 1) return nullptr;
    buf = (uint16_t *)malloc(w * h * 2);
    if (!buf) return nullptr;
    _w = w;
    _h = h;
    memset(buf, 0, w * h * 2);
    return buf;
}

void tft_sprite::deleteSprite() {
    if (buf) free(buf);
    buf = nullptr;
    _w = _h = 0;
}

void tft_sprite::setColorDepth(uint8_t depth) {}

int16_t tft_sprite::width() const { return _w; }
int16_t tft_sprite::height() const { return _h; }

void tft_sprite::drawPixel(int32_t x, int32_t y, uint32_t color) {
    if (!buf || x < 0 || y < 0 || x >= _w || y >= _h) return;
    buf[y * _w + x] = (uint16_t)(color & 0xFFFF);
}

void tft_sprite::fillScreen(uint32_t color) { fillRect(0, 0, _w, _h, color); }

void tft_sprite::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    for (int32_t j = y; j < y + h; j++)
        for (int32_t i = x; i < x + w; i++) drawPixel(i, j, color);
}

void tft_sprite::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
    fillRect(x, y, 1, h, color);
}

void tft_sprite::drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
    fillRect(x, y, w, 1, color);
}

void tft_sprite::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void tft_sprite::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    drawRect(x, y, w, h, color);
}

void tft_sprite::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
    fillRect(x, y, w, h, color);
}

void tft_sprite::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int32_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int32_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;
    while (true) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void tft_sprite::drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    for (int32_t a = 0; a < 360; a += 10) {
        drawPixel(x0 + (int32_t)(r * cosf(a * 0.0174533f)), y0 + (int32_t)(r * sinf(a * 0.0174533f)), color);
    }
}

void tft_sprite::fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
    for (int32_t y = -r; y <= r; y++)
        for (int32_t x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) drawPixel(x0 + x, y0 + y, color);
}

void tft_sprite::fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
    for (int32_t y = -ry; y <= ry; y++) {
        int32_t w = (int32_t)(rx * sqrtf(1.0f - (float)y * y / (float)(ry * ry)));
        drawFastHLine(x0 - w, y0 + y, 2 * w + 1, color);
    }
}

void tft_sprite::drawTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

void tft_sprite::fillTriangle(
    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color
) {
    int32_t miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int32_t maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    int32_t xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
    for (int32_t y = miny; y <= maxy; y++) {
        int32_t xmin = INT32_MAX, xmax = INT32_MIN;
        for (int e = 0; e < 3; e++) {
            int32_t xA = xs[e], yA = ys[e], xB = xs[(e + 1) % 3], yB = ys[(e + 1) % 3];
            if ((yA <= y && y < yB) || (yB <= y && y < yA)) {
                int32_t x = xA + (int32_t)((float)(y - yA) * (xB - xA) / (float)(yB - yA));
                if (x < xmin) xmin = x;
                if (x > xmax) xmax = x;
            }
        }
        if (xmax >= xmin) drawFastHLine(xmin, y, xmax - xmin + 1, color);
    }
}

void tft_sprite::drawXBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
    int32_t byteWidth = (w + 7) / 8;
    for (int16_t j = 0; j < h; j++)
        for (int16_t i = 0; i < w; i++)
            if (bitmap[j * byteWidth + i / 8] & (1 << (i % 8))) drawPixel(x + i, y + j, color);
}

void tft_sprite::setCursor(int16_t x, int16_t y) {
    _cursorX = x;
    _cursorY = y;
}
void tft_sprite::setTextColor(uint16_t c) { setTextColor(c, _textBgColor); }
void tft_sprite::setTextColor(uint16_t c, uint16_t b, bool bgfill) {
    _textColor = c;
    _textBgColor = b;
}
void tft_sprite::setTextSize(uint8_t s) {
    if (s < 1) s = 1;
    _textSize = s;
}
void tft_sprite::setTextDatum(uint8_t d) { _textDatum = d; }

int16_t tft_sprite::drawString(const String &string, int32_t x, int32_t y, uint8_t font) {
    if (!buf) return 0;
    int16_t cx = x, cy = y;
    for (size_t i = 0; i < string.length(); i++) {
        char c = string[i];
        if (c == '\n') {
            cx = x;
            cy += 6 * _textSize;
            continue;
        }
        if (c == ' ') {
            cx += 4 * _textSize;
            continue;
        }
        uint8_t ch = (uint8_t)c;
        if (ch < 32 || ch > 126) ch = ' ';
        uint16_t bits = lm_font3x5[ch - 32];
        for (uint8_t col = 0; col < 3; col++) {
            uint8_t pattern = (bits >> (col * 5)) & 0x1F;
            for (uint8_t row = 0; row < 5; row++)
                for (uint8_t sy = 0; sy < _textSize; sy++)
                    for (uint8_t sx = 0; sx < _textSize; sx++)
                        drawPixel(cx + col * _textSize + sx, cy + row * _textSize + sy, pattern & (1 << row) ? _textColor : _textBgColor);
        }
        cx += 4 * _textSize;
    }
    return cx - x;
}

int16_t tft_sprite::drawCentreString(const String &string, int32_t x, int32_t y, uint8_t font) {
    int16_t w = string.length() * 4 - 1;
    return drawString(string, x - w / 2, y, font);
}

size_t tft_sprite::print(const String &s) {
    if (!buf) return 0;
    size_t n = 0;
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '\n') {
            _cursorX = 0;
            _cursorY += 6 * _textSize;
            n++;
            continue;
        }
        if (c == ' ') {
            _cursorX += 4 * _textSize;
            n++;
            continue;
        }
        drawString(String(c), _cursorX, _cursorY);
        _cursorX += 4 * _textSize;
        n++;
    }
    return n;
}

size_t tft_sprite::println(const String &s) { return print(s + "\n"); }

void tft_sprite::pushSprite(int32_t x, int32_t y, uint32_t transparent) {
    if (!buf || !_parent) return;
    _parent->_suppress = true;
    for (int16_t j = 0; j < _h; j++)
        for (int16_t i = 0; i < _w; i++) {
            uint16_t c = buf[j * _w + i];
            if (transparent != TFT_TRANSPARENT && c == (uint16_t)transparent) continue;
            _parent->drawPixel(x + i, y + j, c);
        }
    _parent->_suppress = false;
    _parent->flushNow();
}

void tft_sprite::pushToSprite(tft_sprite *dest, int32_t x, int32_t y, uint32_t transparent) {
    if (!buf || !dest || !dest->buf) return;
    for (int16_t j = 0; j < _h; j++)
        for (int16_t i = 0; i < _w; i++) {
            uint16_t c = buf[j * _w + i];
            if (transparent != TFT_TRANSPARENT && c == (uint16_t)transparent) continue;
            dest->drawPixel(x + i, y + j, c);
        }
}

void tft_sprite::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *data) {
    for (int32_t j = 0; j < h; j++)
        for (int32_t i = 0; i < w; i++) drawPixel(x + i, y + j, data[j * w + i]);
}

void tft_sprite::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t *data, bool bpp8, uint16_t *cmap) {
    for (int32_t j = 0; j < h; j++)
        for (int32_t i = 0; i < w; i++) {
            uint8_t idx = data[j * w + i];
            drawPixel(x + i, y + j, cmap ? cmap[idx] : idx);
        }
}

void tft_sprite::pushImage(
    int32_t x, int32_t y, int32_t w, int32_t h, const uint8_t *data, bool bpp8, uint16_t *cmap
) {
    pushImage(x, y, w, h, (uint8_t *)data, bpp8, cmap);
}

void tft_sprite::fillRectHGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2) {
    fillRect(x, y, w, h, color1);
}
void tft_sprite::fillRectVGradient(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color1, uint32_t color2) {
    fillRect(x, y, w, h, color1);
}

#endif // USE_LEDMATRIX
