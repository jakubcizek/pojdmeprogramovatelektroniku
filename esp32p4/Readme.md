# Testovací programy pro mikrokontroler ESP32-P4

**Příloha článku „Stíhačka z Asie ESP32-P4“**, kterému se věnuji v časopisu [Computer 10/2025](https://www.ikiosek.cz/computer).

1. program **01_ahoj_svete**: Vypíše informace o čipu a pamětech a spustí někonečnou smyčku se zdravicemi, které budou střídavě vypisovat jednotlivá HP jádra procesoru
2. program **02_ram_benchamrk**: Provede primitivní benchmark čtení a zápisu do 32MB externí SPI PSRAM. A to jak skrze standardní memcpy po malých blocích, tak pomocí DMA a asynchronního memcpy
3. program **03_camera_jpeg_server**: Spustí webový server s ovládací stránkou pro pořizování fotek a aktivaci MJPEG streamu. Zatím pomalé kvůli režii Wi-Fi na sekundárním čipu; je třeba doladit konfiguraci
4. program **04_camera_jpeg_server_ethernet**: Spustí webový server s ovládací stránkou pro pořizování fotek a aktivaci MJPEG streamu. Namísto Wi-Fi používáme rychlejší ethernet a variantu desky s RJ-45 a PHY čipem IP101

- Použitý hardware: [Waveshare ESP32-P4-WIFI](https://www.waveshare.com/esp32-p4-wifi6.htm?sku=32020) (zmenšenína oficiálního devkitu od Espressifu)
- Použitý hardware (4. program): [Waveshare ESP32-P4-ETH](https://www.waveshare.com/esp32-p4-wifi6.htm?sku=32020) (zmenšenína oficiálního devkitu od Espressifu)
- Použitá kamera: [Libovolný klon RPi kamery se snímačem OV5647 a rozhraním MIPI CSI](https://www.waveshare.com/rpi-camera-b.htm)
- Propojovací kabel kamery s užší/Mini koncovkou pro konektor desky: [Raspberry Pi Camera Cable Standard - Mini 200 mm](https://rpishop.cz/mipi/6501-raspberry-pi-5-camera-cable-standard-mini-200-mm.html)
- Vývojové prostředí: [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html)

## fontgenerator.py

Tento skript v Pythonu postavený na knihovně PIL/Pillow slouží ke generování rastrových fontů pro komponentu gfx.h, kteoru používáme ve třetím programu
Příklad použití: 

Zobrazí nápovědu:

```bash
python fontgenerator.py -h
```

Vytvoří rastrové písmo arial30 s 30px znaky z TTF fontu arial do souboru arial30px.h. Použije se výchozí znaková sada (běžné znaky na klávesnici)

```bash
python fontgenerator.py --ttf arial.ttf --size 30 --name arial30 --out arial30px.h
```

Vytváříme jen rastrové znaky/glyfy pro písmena 0-9 a mezeru

```bash
python fontgenerator.py --ttf arial.ttf --size 30 --name arial30 --out arial30px.h --charset "0123456789 "
```

Vytváříme jen rastrové znaky/glyfy pro písmena v souboru text.txt

```bash
python fontgenerator.py --ttf arial.ttf --size 30 --name arial30 --out arial30px.h --charset_file text.txt
```

## Řešení problémů

Překlad kódu proběhne v pořádku, ale Visual Studio Code mi červeným podtržením hlásí neznámé funkce a hlavičkové soubory:

- Vytvořte ve VSC nový projekt (Ctrl+Shift+P -> ESP-IDF: New Project) a poté z adresáře nového projektu do toho stávajícího zkopírujte vygenerovaný .vscode/c_cpp_properties.json.

Soubor bude mít zhruba tuto podobu:

```json
{
  "configurations": [
    {
      "name": "ESP-IDF",
      "compilerPath": "${config:idf.toolsPathWin}\\tools\\xtensa-esp-elf\\esp-14.2.0_20241119\\xtensa-esp-elf\\bin\\xtensa-esp32-elf-gcc.exe",
      "compileCommands": "${config:idf.buildPath}/compile_commands.json",
      "includePath": [
        "${config:idf.espIdfPath}/components/**",
        "${config:idf.espIdfPathWin}/components/**",
        "${workspaceFolder}/**"
      ],
      "browse": {
        "path": [
          "${config:idf.espIdfPath}/components",
          "${config:idf.espIdfPathWin}/components",
          "${workspaceFolder}"
        ],
        "limitSymbolsToIncludedHeaders": true
      }
    }
  ],
  "version": 4
}
```
