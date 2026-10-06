

# :robot: Bruce per Makeblock Codey Rocky

Port del firmware **Bruce** sul robot educativo **Makeblock Codey Rocky**: la **matrice LED 16×8** fa da schermo, i tasti **A/B/C** (+ tasto accensione) navigano i menu e tutti i **sensori del robot** sono usati da Bruce.

Base: fork di [emericklaw/Bruce](https://github.com/emericklaw/Bruce), a monte del progetto originale [BruceDevices/firmware](https://github.com/BruceDevices/firmware).

> [!WARNING]
> **Disclaimer** — Bruce è un tool per operazioni di cyber offensive e red team, distribuito con licenza AGPL. Destinato esclusivamente a test di sicurezza legali e autorizzati: valida le funzioni WiFi/BLE solo su reti e dispositivi di tua proprietà. L'uso per attività dannose o non autorizzate è severamente vietato. Gli sviluppatori non si assumono alcuna responsabilità per usi impropri. Usa a tuo rischio.

## :open_file_folder: Documentazione

- **[boards/codey-rocky/README.md](boards/codey-rocky/README.md)** — documentazione completa del port: tabella hardware (pin, periferiche), navigazione, sensori, note tecniche
- **[Font editor](README_font_editor.md)** — editor interattivo del font 6×8 della matrice (`tools/font_editor.py`)

## :floppy_disk: Installazione (binari precompilati)

Scarica i file dalla release [v1.0-codey-rocky](https://github.com/messinamirko99-design/bruce-codey-rocky/releases/tag/v1.0-codey-rocky) e flasha l'**ESP32 del modulo Codey** (quello con lo schermo) collegandolo via USB:

```sh
esptool.py --chip esp32 --port /dev/ttyACM0 --baud 921600 \
  write_flash 0x1000  bootloader.bin \
             0x8000  partitions.bin \
             0x10000 firmware.bin
```

| File | Indirizzo |
|------|-----------|
| `bootloader.bin` | `0x1000` |
| `partitions.bin` | `0x8000` |
| `firmware.bin` | `0x10000` |

> **Non tenere premuti A o B durante l'accensione/flash**: sono rispettivamente GPIO2 e GPIO12, pin di strapping dell'ESP32 (premuti a freddo possono impedire il boot o corrompere la configurazione flash). Se il robot non parte, staccare la batteria qualche secondo e riaccendere senza tasti premuti.

## :hammer_and_wrench: Compilazione da sorgente

```sh
pio run -e codey-rocky            # firmware completo (4 MB)
pio run -e LAUNCHER_codey-rocky   # variante LITE per partizione M5Launcher
pio run -e codey-rocky -t upload  # build + flash via USB
```

L'environment PlatformIO è definito in [`boards/codey-rocky/codey-rocky.ini`](boards/codey-rocky/codey-rocky.ini) e incluso automaticamente da `platformio.ini` via `extra_configs`.

## :x: Funzioni escluse su questo hardware

La build Codey Rocky (`LITE_VERSION`) esclude le periferiche non presenti sul robot: RF 433/CC1101, NRF24, RFID, GPS, Ethernet, LoRa e script JavaScript.

## :sparkles: Cosa aggiunge questo fork

- **Matrice LED 16×8** (controller classe TM1640, bit-bang su GPIO23/22) come display: menu con testo a scorrimento automatico, barre e icone
- **Font autentici** del firmware Codey originale (85 glifi 6×8, font compatto 3×5) e le **6 animazioni originali** (happy, cry, dispirited, angry, fear, test — menu Sensors → Faces), grazie al driver validato dal progetto *codey-sniffer*
- **Menu Sensors** (CodeyMenu): sensore luce, microfono, potenziometro, IMU MPU6050 con livella a bolla, LED RGB, speaker, batteria e **base Rocky** (motori, sensore linea/colore/grigio/ostacolo) via protocollo Neurons
- **Tasti A/B/C** + tasto accensione (Esc breve, spegnimento tenendo premuto 2 s) con indicatori in barra di stato
- **IR NEC+** (TX/RX) con send/raw/replay e TV-B-Gone nel menu IR di Bruce
- `tools/font_editor.py` per ritoccare il font della matrice

## :keyboard: Navigazione

- **A** = conferma/entra
- **B** = voce successiva
- **C** = voce precedente · **C tenuto** = torna indietro/esci
- **Accensione breve** = Esc · **tenuto 2 s** = spegni

## :clap: Acknowledgements

**Upstream Bruce team:** [@pr3y](https://github.com/pr3y) — firmware originale · [@bmorcelli](https://github.com/bmorcelli) — core e porte dispositivi · e tutti i [contributori del progetto Bruce](https://github.com/BruceDevices/firmware/graphs/contributors).

**Questo port:** driver matrice validato sull'hardware reale grazie a *codey-sniffer* (derivato dal backup 2018 del firmware Makeblock) · porte Codey Rocky, font, animazioni e CodeyMenu di [@messinamirko99-design](https://github.com/messinamirko99-design).
