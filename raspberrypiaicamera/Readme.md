# Raspberry Pi AI Camera – Object Detection Demo

Tento repozitář obsahuje ukázkový projekt pro detekci objektů v reálném čase pomocí **Raspberry Pi AI Camera** (s čipem Sony IMX500). Aplikace běží na Raspberry Pi a poskytuje webové rozhraní pro sledování videa, telemetrie a ovládání AI modelu.

> **Poznámka:** Jedná se o doprovodný ukázkový projekt k článku o Raspberry Pi AI Camera v časopisu **Computer (únor 2026)**. Časopis je k dostání v elektronické i tištěné podobě na webu [iKiosek.cz](https://www.ikiosek.cz/computer).

## 🎯 Hlavní funkce

* **Zpracování obrazu přímo na čipu:** Využívá hardwarovou akceleraci IMX500, takže nezatěžuje CPU Raspberry Pi.
* **Webové rozhraní:**
    * MJPEG stream s nízkou latencí.
    * Vykreslování detekčních boxů (BBox) pomocí HTML5 Canvas.
    * Živý graf jistoty detekce (Confidence score) bez problikávání.
* **Pokročilé vyhledávání:**
    * Možnost filtrovat zobrazované objekty.
    * Našeptávač (autocomplete) na základě načteného slovníku modelu.
    * Podpora vyhledávání více objektů najednou.
* **Interaktivní ovládání:**
    * Tlačítka **START/STOP** pro úplné vypnutí kamery a AI (šetří energii a zahřívání).
    * Posuvník pro nastavení prahu detekce (Threshold) v reálném čase.
    * Modální okno s nápovědou dostupných objektů.

## 🛠 Hardware a Požadavky

* **Raspberry Pi** (Zero 2 W, 3B+, 4 nebo 5)
* **Raspberry Pi AI Camera** (Sony IMX500)
* **Raspberry Pi OS** (Bookworm nebo novější)

## 📦 Instalace

1.  **Aktualizace systému a instalace závislostí:**
    Ujistěte se, že máte nainstalované knihovny pro kameru a stažené AI modely.

    ```bash
    sudo apt update
    sudo apt install imx500-all
    ```

2.  **Instalace Python knihoven:**
    Projekt využívá framework `tornado` pro webový server a WebSocket.

    ```bash
    sudo apt install python3-tornado
    ```

## 🚀 Spuštění

Aplikaci spustíte následujícím příkazem:

```bash
python aiserver.py
```

Ve webové mprohlížeči bude ve vcáhozí mstavu k dispozici na adrese: **http://(IP adresa Raspberry Pi):8000**
