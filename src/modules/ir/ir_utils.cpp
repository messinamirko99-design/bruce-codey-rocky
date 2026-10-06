#include "ir_utils.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"

void setup_ir_pin(int pin, uint8_t mode) {
    if (pin < 0) return;
    if (bruceConfigPins.SDCARD_bus.checkConflict(pin)) sdcardSPI.end();
    // Fully detach any previous peripheral (LEDC/RMT/UART) from this GPIO
    gpio_reset_pin((gpio_num_t)pin);
    pinMode(pin, mode);
    if (mode == OUTPUT) {
        digitalWrite(pin, LED_OFF);
    }
}
