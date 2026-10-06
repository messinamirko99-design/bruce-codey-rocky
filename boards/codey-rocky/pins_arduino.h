#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

/*********************************************************************
** Makeblock Codey Rocky ("Codey" module)
** Port of Bruce firmware - reverse engineered from Makeblock's
** MicroPython port (FFtust/micropython_esp32_local, 2017-2018).
**
** MCU: ESP32 (classic), 4MB flash, no PSRAM
** Display: 16x8 monochrome LED matrix (TM1640 class driver)
** IMU: MPU6050 on I2C1 (SDA=19, SCL=18)
** Buttons: A=2 (active HIGH), B=12 (active HIGH), C=27 (active LOW)
** Power key: 34 (active LOW), power latch: 15 (hold HIGH)
*********************************************************************/

#define CODEY_ROCKY_BOARD
#define CODEY_ROCKY

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t TXD2 = 17; // UART1 to Rocky base co-processor
static const uint8_t RXD2 = 16;

static const uint8_t SDA = 19; // MPU6050 bus
static const uint8_t SCL = 18;

static const uint8_t SS   = -1;
static const uint8_t MOSI = -1;
static const uint8_t MISO = -1;
static const uint8_t SCK  = -1;

static const uint8_t G2  = 2;
static const uint8_t G4  = 4;
static const uint8_t G5  = 5;
static const uint8_t G12 = 12;
static const uint8_t G14 = 14;
static const uint8_t G15 = 15;
static const uint8_t G16 = 16;
static const uint8_t G17 = 17;
static const uint8_t G18 = 18;
static const uint8_t G19 = 19;
static const uint8_t G21 = 21;
static const uint8_t G22 = 22;
static const uint8_t G23 = 23;
static const uint8_t G25 = 25;
static const uint8_t G26 = 26;
static const uint8_t G27 = 27;
static const uint8_t G32 = 32;
static const uint8_t G33 = 33;
static const uint8_t G34 = 34;
static const uint8_t G35 = 35;
static const uint8_t G36 = 36;
static const uint8_t G39 = 39;

static const uint8_t DAC1 = 25; // speaker

static const uint8_t ADC1 = 35; // light sensor
static const uint8_t ADC2 = 36; // sound sensor

// Buttons (GPIO2/12 have external pull-downs and are active HIGH,
// GPIO27 has an external pull-up and is active LOW)
#ifndef HAS_BTN
#define HAS_BTN 1
#endif
#ifndef SEL_BTN
#define SEL_BTN 2  // Button A
#endif
#ifndef DW_BTN
#define DW_BTN 12  // Button B
#endif
#ifndef UP_BTN
#define UP_BTN 27  // Button C
#endif
#ifndef PWR_BTN
#define PWR_BTN 34 // power key (input only, active LOW)
#endif
#ifndef BTN_ACT
#define BTN_ACT HIGH
#endif
#ifndef BTN_ALIAS
#define BTN_ALIAS "\"A\""
#endif

#ifndef DEEPSLEEP_WAKEUP_PIN
#define DEEPSLEEP_WAKEUP_PIN 34
#endif
#ifndef DEEPSLEEP_PIN_ACT
#define DEEPSLEEP_PIN_ACT LOW
#endif

// Infrared: V1.0.7 hardware (TX on 26, RX on 5)
#ifndef TXLED
#define TXLED 26
#endif
#ifndef RXLED
#define RXLED 5
#endif
#ifndef LED_ON
#define LED_ON HIGH
#endif
#ifndef LED_OFF
#define LED_OFF LOW
#endif
#ifndef IR_TX_PINS
#define IR_TX_PINS '{ {"Default", TXLED}, {"G26", 26}, {"G25", 25}, {"G14", 14} }'
#endif
#ifndef IR_RX_PINS
#define IR_RX_PINS '{ {"Default", RXLED}, {"G5", 5}, {"G35", 35}, {"G36", 36} }'
#endif

// RF (one-pin 433 modules can be wired on the exposed pads / grove-ish ports)
#ifndef RF_TX_PINS
#define RF_TX_PINS '{ {"Default", 26}, {"G25", 25}, {"G14", 14} }'
#endif
#ifndef RF_RX_PINS
#define RF_RX_PINS '{ {"Default", 5}, {"G35", 35}, {"G36", 36} }'
#endif

// Battery ADC pin
#ifndef BAT_PIN
#define BAT_PIN 33
#endif

// Buzzer / speaker: DAC pin 25, driven by LEDC square wave (tone())
#ifndef BUZZ_PIN
#define BUZZ_PIN 25
#endif

// Onboard RGB status LED (PWM, R=14 G=21 B=4)
#define CODEY_RGB_R 14
#define CODEY_RGB_G 21
#define CODEY_RGB_B 4

// LED matrix bit-bang pins (TM1640 class driver)
#define CODEY_MATRIX_SCL 23
#define CODEY_MATRIX_SDA 22

// Rocky base (co-processor) UART1
#define ROCKY_UART_TX 17
#define ROCKY_UART_RX 16

// Sensors
#define CODEY_LIGHT_PIN 35 // ADC1_CH7
#define CODEY_SOUND_PIN 36 // ADC1_CH0
#define CODEY_KNOB_PIN 39  // ADC1_CH3
#define CODEY_HWVER_PIN 32 // hardware revision detect

// Default I2C port (same bus as the IMU)
#ifndef GROVE_SDA
#define GROVE_SDA 19
#endif
#ifndef GROVE_SCL
#define GROVE_SCL 18
#endif

// SPI pins exposed for external CC1101/NRF24 modules (untested wiring,
// defaults to the I2C/UART free pads)
#define SPI_SCK_PIN 18
#define SPI_MOSI_PIN 23
#define SPI_MISO_PIN 19
#define SPI_SS_PIN 5

// Display: 16x8 LED matrix
#ifndef HAS_SCREEN
#define HAS_SCREEN 1
#endif
#ifndef USE_LEDMATRIX
#define USE_LEDMATRIX
#endif
#ifndef TINY_DISPLAY
#define TINY_DISPLAY
#endif
#ifndef TFT_WIDTH
#define TFT_WIDTH 16
#endif
#ifndef TFT_HEIGHT
#define TFT_HEIGHT 8
#endif
#ifndef ROTATION
#define ROTATION 0
#endif

// Font sizes (only one size is really usable on the matrix)
#ifndef FP
#define FP 1
#endif
#ifndef FM
#define FM 1
#endif
#ifndef FG
#define FG 1
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME '"Codey Rocky"'
#endif

#endif /* Pins_Arduino_h */
