# Font editor (Codey Rocky / Bruce)

Editor interattivo del font `CHAR_FONT` (6×8) usato sulla matrice LED.

## Avvio

```bash
# dalla root del repo Bruce (dopo unzip del fix):
python3 tools/font_editor.py lib/HAL/display/codey_fonts.h

# oppure percorso assoluto:
python3 /path/to/font_editor.py /path/to/bruce/lib/HAL/display/codey_fonts.h
```

Richiede un terminale vero (non tutti i pannelli IDE):
```bash
# se sei in un IDE, apri un terminale esterno:
cd ~/.zcode/workspace/default/bruce
python3 tools/font_editor.py lib/HAL/display/codey_fonts.h
```

## Controlli

| Tasto | Azione |
|-------|--------|
| ←↑↓→ | Muove il cursore sul pixel |
| Spazio | Accende/spegne il pixel |
| `n` / `p` | Carattere successivo / precedente |
| `A`–`Z`, `0`–`9`… | Salta a quel glifo |
| `c` | Pulisce il glifo |
| `i` | Inverte |
| `f` / `h` | Flip verticale / orizzontale |
| `s` | **Salva** in `codey_fonts.h` |
| `q` | Esci (`Q` forza uscita senza salvare) |

Dopo `s`, ricompila e flasha:

```bash
pio run -e codey-rocky -t upload
```
