# Bruce per Makeblock Codey Rocky

Port del firmware **Bruce** (fork `emericklaw/Bruce` di BruceDevices/firmware) sul
robot educativo **Makeblock Codey Rocky**, usando la **matrice LED 16×8 come
schermo**, i **tasti A/B/C** (+ tasto accensione) per navigare e tutti i
**sensori a bordo**.

> ⚠️ Il firmware Bruce è destinato esclusivamente a test di sicurezza autorizzati.
> Valida le funzioni WiFi/BLE solo su reti e dispositivi di tua proprietà.

## Credito: driver validato sull'hardware reale

Il livello basso del driver della matrice (sequenza di trasmissione, timing
di 3 µs, flag di orientamento) è copiato dal progetto **codey-sniffer**
testato sul robot reale, che a sua volta deriva dal backup 2018 del firmware
Makeblock. Da lì provengono anche i **font autentici** (85 glifi 6×8 usati
dal font grande via `setTextSize(2)`, font numeri 3×8) e le **6 animazioni
originali** (happy, cry, dispirited, angry, fear, test) — riproducibili dal
menu Sensors → Faces.

## Hardware supportato in questo port

| Periferica | Dettagli | Come si usa |
|---|---|---|
| Matrice LED 16×8 | Controller classe TM1640, bit-bang su GPIO23 (SCL) / GPIO22 (SDA) | Schermo: menu con testo a scorrimento, barre, icone |
| Tasto A (GPIO2) | attivo ALTO | **Select** |
| Tasto B (GPIO12) | attivo ALTO | **Next** (giù) |
| Tasto C (GPIO27) | attivo BASSO | **Prev** (su) — pressione lunga = **Esc/Back** |
| Tasto accensione (GPIO34) | attivo BASSO | **Esc** breve, tenuto 2 s = **spegnimento** |
| Sensore luce (GPIO35) | ADC1_CH7 | Menu Sensors → Light |
| Microfono (GPIO36) | ADC1_CH0, picco-picco 50 ms | Menu Sensors → Sound |
| Potenziometro (GPIO39) | ADC1_CH3 | Menu Sensors → Knob |
| IMU MPU6050 | I2C SDA=19 SCL=18, addr 0x68 | Menu Sensors → IMU (livella a bolla) |
| LED RGB | PWM LEDC R=14 (invertito), G=21, B=4 | Menu Sensors → RGB Led |
| Speaker | DAC/LEDC su GPIO25 | Menu Sensors → Speaker, beep di sistema |
| IR TX/RX | TX=GPIO26, RX=GPIO5 (38 kHz, NEC+) | Menu IR di Bruce: send/raw/replay, TV-B-Gone |
| Batteria | GPIO33, `V = mV_pin×2/1000 + 0.2` (catena originale: counts×0.001934+0.2) | Indicatore in barra di stato |
| Base Rocky | UART1 115200, TX=17 RX=16, protocollo "Neurons" (Makeblock) | Menu Sensors → Rocky Base: guida motori, sensore linea/colore/grigio/ostacolo |
| WiFi/BLE | ESP32 classico | Menu Wifi/Ble di Bruce: deauth, beacon spam, Evil Portal, WebUI, BLE spam, Bad-BLE… |

I menu non implementabili su questo hardware (RF 433/CC1101, NRF24, RFID, GPS,
Ethernet, LoRa, script JS) sono esclusi dalla build di Codey Rocky.

## Navigazione

- **B** = voce successiva
- **C** = voce precedente · **C tenuto** = torna indietro / esci
- **A** = conferma/entra
- **Tasto accensione breve** = Esc · **tenuto 2 s** = spegni
- Testo più lungo dello schermo scorre automaticamente (stile firmware originale Codey).

## Compilazione

```bash
pio run -e codey-rocky            # firmware completo (4 MB)
pio run -e LAUNCHER_codey-rocky   # variante LITE per partizione M5Launcher
```

## Flash

Il Codey Rocky ha due MCU; si flasha **solo l'ESP32 del modulo Codey** (quello
con lo schermo). Collega il cavo USB e flasha normalmente (l'auto-reset USB
gestisce la download mode):

```bash
pio run -e codey-rocky -t erase && pio run -e codey-rocky -t upload
```

oppure, manualmente con esptool:

```bash
esptool.py --chip esp32 --port /dev/ttyACM0 --baud 921600 \
  write_flash 0x1000 .pio/build/codey-rocky/bootloader.bin \
             0x8000 .pio/build/codey-rocky/partitions.bin \
             0xe000 .pio/build/codey-rocky/boot_app0.bin \
             0x10000 .pio/build/codey-rocky/firmware.bin
```

**Non tenere premuti A o B durante l'accensione/flash**: sono rispettivamente
GPIO2 e GPIO12, pin di strapping dell'ESP32 (premuti a freddo possono impedire
il boot o corrompere la configurazione flash). Se il robot non parte, staccare
la batteria qualche secondo e riaccendere senza tasti premuti.

## File principali del port

- `boards/codey-rocky/pins_arduino.h` — pinout completo
- `boards/codey-rocky/interface.cpp` — tasti, batteria, latch accensione, sleep
- `boards/codey-rocky/codey_hw.h/.cpp` — driver sensori + protocollo Neurons
- `boards/codey-rocky/codey-rocky.ini` — environment PlatformIO
- `lib/HAL/display/ledmatrix.h/.cpp` — backend display (TM1640, font 3×5, sprite)
- `src/core/menu_items/CodeyMenu.h/.cpp` — menu "Sensors"
- `src/core/display.cpp` / `src/main.cpp` — ramo `TINY_DISPLAY` della UI

## Note tecniche

Implementazione fedele al sorgente MicroPython originale (algoritmi della
guida di riferimento):

- **Sensori analogici** (luce/suono/knob/batteria): 10 letture ADC, scarto di
  max e min, media sugli 8 centrali, risoluzione 12 bit e attenuazione 11 dB
  come l'originale; percentuale = media × 100/4095.
- **Batteria**: `V = mV_pin/1000 × 2 + 0.2` — la costante originale
  `0.001934 = (1/4096)×1.1×3.6×2` contiene già il guadagno dell'attenuazione,
  che `analogReadMilliVolts()` include: il fattore corretto è ×2 (partitore
  1:2) più l'offset di 0.2 V dell'originale. Percentuale lineare ~3.5–4.4 V
  (≈3.3–4.2 V reali), da verificare sul robot.
- **MPU6050**: sequenza init originale con i suoi delay (0x6B=0 → 200 ms,
  0x1A=0x01 → 100 ms, 0x1B=0x08 → 100 ms, 0x19=19), WHO_AM_I atteso 0x98
  (clone Makeblock; accettato anche 0x68), calibrazione giroscopio a 500
  campioni scartando i primi/ultimi 50, angoli con filtro esponenziale 0.8/0.2,
  assi X/Y invertiti, roll finale negato (formule pitch/roll originali con il
  ramo per il robot capovolto).
- **Speaker**: DAC interno canale 1 (GPIO25) via `dacWrite()` — il percorso
  "8bit_voice" dell'originale — con onda quadra scritta sul DAC, non LEDC.
- **LED RGB**: nell'origine è un LED PWM a 3 canali LEDC (R invertito,
  intensità 0-100), NON una striscia WS2812: la periferica RMT del Codey è
  usata esclusivamente dal driver IR NEC (verificato in `codey_rmt_board.c`).
- **Orientamento matrice**: identico al progetto testato (bit y = riga y,
  LSB-first). Se il display risultasse specchiato/capovolto, definire
  `MATRIX_FLIP_X` / `MATRIX_FLIP_Y` = 1 nei build flag di
  `boards/codey-rocky/codey-rocky.ini`.
- **Protocollo matrice**: nel frame il byte d'indirizzo `0xC0` e i 16 byte di
  dati condividono UNA sola trasmissione (stop solo alla fine) — è la
  sequenza validata; inviare lo stop dopo l'indirizzo lascia il pannello
  spento (bug presente nella prima versione di questo port).

- Il latch di accensione (GPIO15) viene forzato ALTO da un costruttore globale,
  prima di `setup()`, per evitare lo spegnimento al rilascio del tasto.
- L'orientamento della matrice (riga 0 in alto) segue l'API `show_pixel` del
  firmware MicroPython originale; se sull'hardware reale risultasse capovolto,
  basta invertire i bit in `LedMatrix::pushFrame`.
- I valori sensori della base Rocky arrivano via report periodici (200 ms) del
  protocollo Neurons (`service 0x63 / sub 0x10`, checksum `&0x7f`).
