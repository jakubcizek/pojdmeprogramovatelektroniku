# VLM Chat - Raspberry Pi + Hailo-10H

> Příloha článku o akcelerační desce **Raspberry Pi AI HAT+ 2**, který vyšel v časopisu **Computer** (květen 2026).

Webový chat s vizuálně-jazykovým modelem (VLM) běžící na Raspberry Pi 5 s AI akcelerátorem Hailo-10H. Aplikace zachytává obraz z kamery, zobrazuje ho jako live MJPEG stream a umožňuje klást otázky o tom, co kamera vidí.

Model **Qwen2-VL-2B-Instruct** běží lokálně na Hailo-10H akcelerátoru -- žádná data se neodesílají do cloudu.

## Co to umí

- Live náhled z RPi kamery (MJPEG stream)
- Chat s VLM modelem -- položíte otázku, model popíše co vidí na obraze
- Streamované odpovědi po částech (chunked WebSocket)
- Automatické stažení modelu (~2.2 GB) při prvním spuštění
- Webové rozhraní přístupné z libovolného zařízení v síti

## Požadavky

### Hardware

- **Raspberry Pi 5** (4 GB RAM nebo více)
- **Hailo-10H** AI akcelerátor (M.2 modul + HAT)
- **RPi kamera** (např. Camera Module 3 s IMX708)

### Software

#### 1. Hailo akcelerátor a HailoRT

Nainstalujte ovladače Hailo akcelerátoru a HailoRT podle oficiálního návodu Raspberry Pi:

https://www.raspberrypi.com/documentation/computers/ai.html

Tím získáte balíčky `hailo_platform` a `hailo_platform.genai`, které skript používá pro inferenci na Hailo-10H.

Ověření instalace:

```bash
hailortcli fw-control identify
```

#### 2. Picamera2

Knihovna pro ovládání RPi kamery. Na Raspberry Pi OS je obvykle předinstalovaná:

```bash
sudo apt install -y python3-picamera2
```

#### 3. Tornado

Webový framework pro HTTP server a WebSocket komunikaci:

```bash
pip install tornado
```

#### 4. OpenCV

Používá se pro změnu velikosti obrazu, konverzi barevných prostorů a JPEG kódování:

```bash
sudo apt install -y python3-opencv
```

## Spuštění

```bash
python vlm_chat.py
```

Při prvním spuštění se automaticky stáhne VLM model (~2.2 GB). Poté server vypíše adresu, na které je dostupný:

```
VLM server bezi na portu 8080:
  http://192.168.1.100:8080/
```

Otevřete tuto adresu v prohlížeči na libovolném zařízení v lokální síti.

Server ukončíte pomocí `Ctrl+C`.

## Konfigurace

Hlavní parametry lze upravit přímo na začátku souboru `vlm_server.py`:

| Parametr | Výchozí | Popis |
|---|---|---|
| `PORT` | `8080` | TCP port HTTP serveru |
| `MAX_TOKENS` | `200` | Max. počet generovaných tokenů (délka odpovědi) |
| `TEMPERATURE` | `0.1` | Míra náhodnosti (0.0 = deterministický, 1.0 = kreativní) |
| `SEED` | `42` | Seed pro reprodukovatelnost (změnit pro jiný výstup) |
| `MJPEG_FPS` | `15` | Snímková frekvence live streamu |
| `MJPEG_QUALITY` | `70` | Kvalita JPEG komprese (0-100) |

## Struktura projektu

```
vlm_chat.py   # Hlavní skript serveru
index.html      # Webové rozhraní (HTML/CSS/JS)
```

## Řešení problémů

### Kamera je obsazena

```
CHYBA: Kamera je obsazena jinym procesem.
```

Zkontrolujte, který proces kameru blokuje:

```bash
ps aux | grep -E 'libcamera|picamera|vlm_server'
```

A ukončete ho pomocí `kill <PID>`.

### Model se nestáhne

Skript stahuje model automaticky ze serveru Hailo. Pokud stažení selže, zkontrolujte připojení k internetu a zda je `hailortcli` dostupný v PATH.
