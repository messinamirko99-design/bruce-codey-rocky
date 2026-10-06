/**
 * @file ir_read.cpp
 * @author @im.nix (https://github.com/Niximkk)
 * @author Rennan Cockles (https://github.com/rennancockles)
 * @brief Read IR signals
 * @version 0.2
 * @date 2024-08-03
 */

#include "ir_read.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/settings.h"
#include "custom_ir.h"
#include "ir_utils.h"
#include <IRrecv.h>
#include <IRutils.h>
#include <globals.h>

#define IR_FREQUENCY 38000
#define DUTY_CYCLE 0.330000

String uint32ToString(uint32_t value) {
    char buffer[12] = {0};
    snprintf(
        buffer,
        sizeof(buffer),
        "%02lX %02lX %02lX %02lX",
        value & 0xFF,
        (value >> 8) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 24) & 0xFF
    );
    return String(buffer);
}

String uint32ToStringInverted(uint32_t value) {
    char buffer[12] = {0};
    snprintf(
        buffer,
        sizeof(buffer),
        "%02lX %02lX %02lX %02lX",
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF
    );
    return String(buffer);
}

IrRead::IrRead(bool headless_mode, bool raw_mode) {
    headless = headless_mode;
    raw = raw_mode;
    setup();
}
bool quickloop = false;

static String getParsedProtocolName(const decode_results &r) {
    switch (r.decode_type) {
        case decode_type_t::RC5: return (r.command > 0x3F) ? "RC5X" : "RC5";
        case decode_type_t::RC6: return "RC6";
        case decode_type_t::SAMSUNG: return "Samsung32";
        case decode_type_t::SONY:
            if (r.address > 0xFF) return "SIRC20";
            if (r.address > 0x1F) return "SIRC15";
            return "SIRC";
        case decode_type_t::NEC:
            if (r.address > 0xFFFF) return "NEC42ext";
            if (r.address > 0xFF1F) return "NECext";
            if (r.address > 0xFF) return "NEC42";
            return "NEC";
        case decode_type_t::UNKNOWN: return "";
        default: return typeToString(r.decode_type, r.repeat);
    }
}

void IrRead::setup() {
#ifdef USE_BOOST
    PPM.enableOTG();
#endif
    // Ensure RX pin is a known IR_RX_PINS entry (defaults to RXLED=5 on Codey)
    const std::vector<std::pair<String, int>> pins = IR_RX_PINS;
    int count = 0;
    for (auto pin : pins) {
        if (pin.second == bruceConfigPins.irRx) count++;
    }
    if (count == 0) gsetIrRxPin(true);

    // Configure RX GPIO then start the receiver (pin already set in ctor
    // from bruceConfigPins.irRx default = RXLED).
    setup_ir_pin(bruceConfigPins.irRx, INPUT); // no pull-up: demodulator drives the line
    irrecv.enableIRIn();
    if (headless) return;
    returnToMenu = true;
    std::vector<Option> quickRemoteOptions = {
        {"TV",
         [&]() {
             quickButtons = quickButtonsTV;
             begin();
             return loop();
         }                     },
        {"AC",
         [&]() {
             quickButtons = quickButtonsAC;
             begin();
             return loop();
         }                     },
        {"FAN",
         [&]() {
             quickButtons = quickButtonsFAN;
             begin();
             return loop();
         }                     },
        {"SOUND",
         [&]() {
             quickButtons = quickButtonsSOUND;
             begin();
             return loop();
         }                     },
        {"LED STRIP", [&]() {
             quickButtons = quickButtonsLED;
             begin();
             return loop();
         }},
    };
    options = {
        {"Custom Read",
         [&]() {
             begin();
             return loop();
         }                            },
        {"Quick Remote Setup  ",
         [&]() {
             quickloop = true;
             loopOptions(quickRemoteOptions);
         }                            },
        {"Menu",                 yield},
    };
    loopOptions(options);
}

void IrRead::loop() {
    while (1) {
        if (check(EscPress)) {
            returnToMenu = true;
            button_pos = 0;
            quickloop = false;
            irrecv.disableIRIn(); // free RMT / GPIO for next use
#ifdef USE_BOOST
            PPM.disableOTG();
#endif
            break;
        }

        if (_emulate_mode) {
            if (check(SelPress)) emulate_signal();
            if (check(NextPress)) {
                _emulate_mode = false;
                discard_signal();
            }
            if (check(PrevPress)) {
                _emulate_mode = false;
                save_signal();
            }
        } else {
            if (check(NextPress)) save_signal();
            if (quickloop && button_pos == quickButtons.size()) save_device();
            if (check(SelPress)) {
                if (_read_signal) emulate_signal();
                else save_device();
            }
            if (check(PrevPress)) discard_signal();
            read_signal();
        }
    }
}

void IrRead::begin() {
    _read_signal = false;

    display_banner();
#ifndef TINY_DISPLAY
    if (quickloop) {
        padprintln("Waiting for signal of button: " + String(quickButtons[button_pos]));
    } else {
        padprintln("Waiting for signal...");
    }

    tft.println("");
    display_btn_options();
#endif

    delay(300);
}

void IrRead::cls() {
#ifdef TINY_DISPLAY
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
#else
    drawMainBorder();
    tft.setCursor(10, 28);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
#endif
}

void IrRead::display_banner() {
    cls();
#ifdef TINY_DISPLAY
    // 16x8 text-only status
    if (_read_signal) {
        String proto = getParsedProtocolName(results);
        if (proto.length() == 0) proto = "RAW";
        proto.toUpperCase();
        tft.drawString(proto.substring(0, 4), 0, 0);
    } else if (quickloop && button_pos < (int)quickButtons.size()) {
        String b = quickButtons[button_pos];
        b.toUpperCase();
        tft.drawString(b.substring(0, 4), 0, 0);
    } else {
        tft.drawString("WAIT", 0, 0);
    }
    tft.flushNow();
#else
    tft.setTextSize(FM);
    padprintln("IR Read");

    tft.setTextSize(FP);
    padprintln("--------------");
    padprintln("Signals captured: " + String(signals_read));
    tft.println("");
#endif
}

void IrRead::display_btn_options() {
#ifdef TINY_DISPLAY
    // Buttons are the physical A/B/C — no room for help text
    return;
#else
    tft.println("");
    tft.println("");
    if (_emulate_mode) {
        padprintln("Press [OK]   to send again");
        padprintln("Press [NEXT] for new signal");
        padprintln("Press [PREV] to save signal");
    } else if (_read_signal) {
        padprintln("Press [OK]   to emulate signal");
        padprintln("Press [NEXT] to save signal");
        padprintln("Press [PREV] to discard");
    } else {
        if (signals_read > 0) { padprintln("Press [OK]   to save device"); }
    }
    padprintln("Press [ESC]  to exit");
#endif
}

void IrRead::read_signal() {
    if (_read_signal || !irrecv.decode(&results)) return;

    _read_signal = true;

    raw = (results.decode_type == decode_type_t::UNKNOWN) || hasACState(results.decode_type);

    String raw_signal = parse_raw_signal();
    _captured_raw_signal = raw_signal;

#ifdef TINY_DISPLAY
    // Codey Rocky: as soon as a valid signal arrives, save it to LittleFS
    // with a generic name and keep listening for the next one.
    auto_save_littlefs();
#else
    display_banner();
    if (!raw) {
        String proto = getParsedProtocolName(results);
        padprintln("Protocol: " + (proto.length() ? proto : "UNKNOWN"));
        padprintln("Bits: " + String(results.bits));
    }

    padprint("RAW Data Captured:");
    tft.println(raw_signal.substring(0, 45) + (raw_signal.length() > 45 ? "..." : ""));

    display_btn_options();
    delay(300);
#endif
}

void IrRead::auto_save_littlefs() {
    // Always store RAW timings so replay is bit-exact (parsed NEC/etc can
    // lose information or use the wrong endianness on send).
    raw = true;
    strDeviceContent = "";
    String btn = "BTN" + String(signals_read);
    append_to_file_str(btn);

    // Pick next free generic filename: IR_001.ir, IR_002.ir, ...
    if (!LittleFS.exists("/BruceIR")) LittleFS.mkdir("/BruceIR");
    int n = 1;
    String filename;
    do {
        char buf[16];
        snprintf(buf, sizeof(buf), "IR_%03d", n);
        filename = String(buf);
        n++;
    } while (LittleFS.exists("/BruceIR/" + filename + ".ir") && n < 1000);

    // Refuse empty payloads (decode overflow / zero-length raw)
    if (strDeviceContent.indexOf("data:") < 0 || strDeviceContent.length() < 20) {
        Serial.println("IR auto-save: empty/invalid payload, skip");
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.setTextSize(1);
        tft.drawString("BAD", 0, 0);
        tft.flushNow();
        delay(500);
        _read_signal = false;
        _captured_raw_signal = "";
        irrecv.resume();
        begin();
        return;
    }

    File file = LittleFS.open("/BruceIR/" + filename + ".ir", FILE_WRITE);
    if (!file) {
        tft.fillScreen(bruceConfig.bgColor);
        tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
        tft.setTextSize(1);
        tft.drawString("ERR", 0, 0);
        tft.flushNow();
        delay(500);
        _read_signal = false;
        _captured_raw_signal = "";
        irrecv.resume();
        begin();
        return;
    }

    file.println("Filetype: Bruce IR File");
    file.println("Version: 1");
    file.println("#");
    file.println("# " + filename);
    file.print(strDeviceContent);
    file.close();

    signals_read++;
    strDeviceContent = "";

    // Brief confirmation on the matrix, then keep listening
    tft.fillScreen(bruceConfig.bgColor);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.setTextSize(1);
    tft.drawString("SAVE", 0, 0);
    tft.flushNow();
    delay(600);

    _read_signal = false;
    _emulate_mode = false;
    _captured_raw_signal = "";
    irrecv.resume();
    begin(); // back to WAIT
}

void IrRead::discard_signal() {
    if (!_read_signal) return;
    _emulate_mode = false;
    _captured_raw_signal = "";
    irrecv.resume();
    begin();
}

void IrRead::emulate_signal() {
    IRCode code;
    if (raw) {
        code.type = "raw";
        code.frequency = IR_FREQUENCY;
        code.data = _captured_raw_signal;
    } else {
        code.type = "parsed";
        code.protocol = getParsedProtocolName(results);
        code.address = uint32ToString(results.address);
        code.command = uint32ToString(results.command);
        code.bits = results.bits;
        code.data = resultToHexidecimal(&results);
        if (code.protocol == "") {
            code.type = "raw";
            code.frequency = IR_FREQUENCY;
            code.data = _captured_raw_signal;
        }
    }
    sendIRCommand(&code);
    if (code.type == "parsed" &&
        (code.protocol == "RC5" || code.protocol == "RC5X" || code.protocol == "RC6")) {
        delay(35);
        sendIRCommand(&code);
    }
    _emulate_mode = true;
    display_banner();
    tft.setTextSize(FP);
    padprintln("Signal emulated!");
    display_btn_options();
}

void IrRead::save_signal() {
    if (!_read_signal) return;
    if (!quickloop) {
        String btn_name = keyboard("Btn" + String(signals_read), 30, "Btn name:");
        if (btn_name == "\x1B") return;
        append_to_file_str(btn_name);
    } else {
        append_to_file_str(quickButtons[button_pos]);
    }
    signals_read++;
    if (quickloop) button_pos++;
    discard_signal();
    delay(100);
}

String IrRead::parse_state_signal() {
    String r = "";
    uint16_t state_len = (results.bits) / 8;
    for (uint16_t i = 0; i < state_len; i++) {
        r += ((results.state[i] < 0x10) ? "0" : "");
        r += String(results.state[i], HEX) + " ";
    }
    r.toUpperCase();
    return r;
}

String IrRead::parse_raw_signal() {

    rawcode = resultToRawArray(&results);
    raw_data_len = getCorrectedRawLength(&results);

    String signal_code = "";

    for (uint16_t i = 0; i < raw_data_len; i++) { signal_code += String(rawcode[i]) + " "; }

    delete[] rawcode;
    rawcode = nullptr;
    signal_code.trim();

    return signal_code;
}

void IrRead::append_to_file_str(String btn_name) {
    strDeviceContent += "name: " + btn_name + "\n";

    if (raw) {
        strDeviceContent += "type: raw\n";
        strDeviceContent += "frequency: " + String(IR_FREQUENCY) + "\n";
        strDeviceContent += "duty_cycle: " + String(DUTY_CYCLE) + "\n";
        strDeviceContent += "data: " + parse_raw_signal() + "\n";
    } else {
        strDeviceContent += "type: parsed\n";
        switch (results.decode_type) {
            case decode_type_t::RC5: {
                if (results.command > 0x3F) strDeviceContent += "protocol: RC5X\n";
                else strDeviceContent += "protocol: RC5\n";
                break;
            }
            case decode_type_t::RC6: {
                strDeviceContent += "protocol: RC6\n";
                break;
            }
            case decode_type_t::SAMSUNG: {
                strDeviceContent += "protocol: Samsung32\n";
                break;
            }
            case decode_type_t::SONY: {
                if (results.address > 0xFF) strDeviceContent += "protocol: SIRC20\n";
                else if (results.address > 0x1F) strDeviceContent += "protocol: SIRC15\n";
                else strDeviceContent += "protocol: SIRC\n";
                break;
            }
            case decode_type_t::NEC: {
                if (results.address > 0xFFFF) strDeviceContent += "protocol: NEC42ext\n";
                else if (results.address > 0xFF1F) strDeviceContent += "protocol: NECext\n";
                else if (results.address > 0xFF) strDeviceContent += "protocol: NEC42\n";
                else strDeviceContent += "protocol: NEC\n";
                break;
            }
            case decode_type_t::UNKNOWN: {
                Serial.print("unknown protocol, try raw mode");
                return;
            }
            default: {
                strDeviceContent += "protocol: " + typeToString(results.decode_type, results.repeat) + "\n";
                break;
            }
        }

        strDeviceContent += "address: " + uint32ToString(results.address) + "\n";
        strDeviceContent += "command: " + uint32ToString(results.command) + "\n";

        strDeviceContent += "bits: " + String(results.bits) + "\n";
        if (hasACState(results.decode_type)) strDeviceContent += "state: " + parse_state_signal() + "\n";
        else if (results.bits > 32)
            strDeviceContent +=
                "value: " + uint32ToString(results.value) + " " + uint32ToString(results.value >> 32) + "\n";
        else strDeviceContent += "value: " + uint32ToStringInverted(results.value) + "\n";
    }
    strDeviceContent += "#\n";
}

void IrRead::save_device() {
    if (signals_read == 0) return;

    String filename = keyboard("MyDevice", 30, "File name:");
    if (filename == "\x1B") return;

    display_banner();

    FS *fs = nullptr;

    bool sdCardAvailable = setupSdCard();
    bool littleFsAvailable = checkLittleFsSize();

    if (sdCardAvailable && littleFsAvailable) {
        options = {
            {"SD Card",  [&]() { fs = &SD; }      },
            {"LittleFS", [&]() { fs = &LittleFS; }},
        };

        loopOptions(options);
    } else if (sdCardAvailable) {
        fs = &SD;
    } else if (littleFsAvailable) {
        fs = &LittleFS;
    };

    if (fs && write_file(filename, fs)) {
        displaySuccess("File saved to " + String((fs == &SD) ? "SD Card" : "LittleFS") + ".", true);
        signals_read = 0;
        strDeviceContent = "";
    } else displayError(fs ? "Error writing file." : "No storage available.", true);

    delay(1000);

    irrecv.resume();
    begin();
}

String IrRead::loop_headless(int max_loops) {

    while (!irrecv.decode(&results)) {
        max_loops -= 1;
        if (max_loops <= 0) {
            Serial.println("timeout");
            return "";
        }
        delay(1000);
    }

    irrecv.disableIRIn();

    if (!raw && results.decode_type == decode_type_t::UNKNOWN) {
        Serial.println("# decoding failed, try raw mode");
        return "";
    }

    if (results.overflow) displayWarning("buffer overflow, data may be truncated", true);

    String r = "Filetype: IR signals file\n";
    r += "Version: 1\n";
    r += "#\n";
    r += "#\n";

    strDeviceContent = "";
    append_to_file_str("Unknown");
    r += strDeviceContent;

    return r;
}

bool IrRead::write_file(String filename, FS *fs) {
    if (fs == nullptr) return false;

    if (!(*fs).exists("/BruceIR")) (*fs).mkdir("/BruceIR");

    while ((*fs).exists("/BruceIR/" + filename + ".ir")) {
        int ch = 1;
        int i = 1;

        displayWarning("File \"" + String(filename) + "\" already exists", true);
        display_banner();

        options = {
            {"Append number", [&]() { ch = 1; }},
            {"Overwrite ",    [&]() { ch = 2; }},
            {"Change name",   [&]() { ch = 3; }},
        };

        loopOptions(options);

        switch (ch) {
            case 1:
                filename += "_";
                while ((*fs).exists("/BruceIR/" + filename + String(i) + ".ir")) i++;
                filename += String(i);
                break;
            case 2: (*fs).remove("/BruceIR/" + filename + ".ir"); break;
            case 3:
                filename = keyboard(filename, 30, "File name:");
                if (filename == "\x1B") return false;
                display_banner();
                break;
        }
    }

    File file = (*fs).open("/BruceIR/" + filename + ".ir", FILE_WRITE);

    if (!file) { return false; }

    file.println("Filetype: Bruce IR File");
    file.println("Version: 1");
    file.println("#");
    file.println("# " + filename);
    file.print(strDeviceContent);

    file.close();
    delay(100);
    return true;
}
