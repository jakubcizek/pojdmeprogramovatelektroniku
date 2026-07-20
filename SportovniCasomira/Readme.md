TBD
# Sportovní časomíra pro amatérské závody

Sportovní časomíra (nejen) pro amatérské cyklistické závody, postavená na optické závoře
E3F-R2N2. Změří čas kola s přesností na milisekundy a výsledky zobrazuje na
malém OLED displeji i ve webovém rozhraní.

> 📖 Tento projekt je **přílohou časopisu Computer (srpen 2026)**. Podrobný popis
> zapojení, principu i kódu najdete v článku — toto README slouží jen jako
> rychlý úvod pro zprovoznění.

## Co budete potřebovat

- Vývojovou desku **ESP32-S3-DevKitC-1**
- OLED displej (SSD1306 0,96" nebo SH1106 1,3"), optickou závoru E3F-R2N2
  a tlačítko — podle schématu v článku
- Prohlížeč **Chrome** nebo **Edge** (kvůli Web Bluetooth / Web Serial)

## ⚠️ Není to Arduino

Firmware je napsaný pro **ESP-IDF** (oficiální vývojové prostředí Espress­ifu),
**nikoli pro Arduino**. Než začnete, je potřeba ESP-IDF nainstalovat.

Nejjednodušší cesta (doporučeno pro začátečníky) je **rozšíření ESP-IDF do
Visual Studio Code**:

1. Nainstalujte [Visual Studio Code](https://code.visualstudio.com/).
2. V záložce Extensions najděte a nainstalujte rozšíření **Espressif IDF**.
3. Projeďte průvodce (**ESP-IDF: Configure Extension**), který vám ESP-IDF
   stáhne a nastaví.

Návod od Espressifu:
<https://github.com/espressif/vscode-esp-idf-extension/blob/master/docs/tutorial/install.md>

## Nahrání firmwaru

V otevřeném projektu ve VS Code:

1. Vyberte cílový čip **ESP32-S3** a svůj sériový port (COM).
2. Klikněte na **Build, Flash and Monitor** (ikona 🔥 v dolní liště).

Případně z příkazové řádky ESP-IDF:

```bash
idf.py build flash monitor
```

První nahrání musí proběhnout **přes USB kabel**. Další aktualizace už lze
provést bezdrátově přes webové rozhraní (OTA).

## Webové rozhraní

Ovládací pult a tabule s výsledky běží v souboru `casomira.html`. Spustíte ho
lokálním serverem (Web Bluetooth i Web Serial vyžadují `http://localhost`,
ne `file://`):

```bash
python server.py
```

Skript otevře stránku v prohlížeči. K desce se pak připojíte přes **USB**
nebo bezdrátově přes **Bluetooth LE** a můžete časomíru ovládat, sledovat
žebříček a exportovat výsledky.

## Licence

Viz článek v časopisu Computer (srpen 2026).
