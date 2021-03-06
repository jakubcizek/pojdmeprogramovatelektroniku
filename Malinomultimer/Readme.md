## Čtyřkanálový Malinomultiměr (voltmetr, ampérmetr a wattmetr na bázi INA219)
Tentokrát si postavíme čtyřkanálový voltmetr/ampérmetr/wattmetr. Jako základní počítač poslouží Raspberry Pi Zero 2 W a jako měřící obvod rozšiřující deska Waveshare Current/Power Monitor HAT se čtyřmi čipy INA219. Stanici dále vybavíme 20×4 znakovým displejem, plochým modulem se čtyřmi tlačítky a vše to udrží pohromadě stativ vyrobený na 3D tiskárně. Jelikož vše běží na výkonném linuxovém počítači, jako programovací jazyk použijeme Python 3. Aplikace multimetr.py si pomocí knihovny Tornado spustí také webový/websocketový server, skrze který bude streamovat údaje z čidel rovnou na webovou stránku.

Prerekvizity: Raspberry Pi OS, Python 3, python3-smbus

 - **[Článek na Živě.cz](https://www.zive.cz/clanky/programovani-elektroniky-promenime-raspberry-pi-zero-2-w-ve-ctyrkanalovy-multimetr-s-wi-fi/sc-3-a-213694/default.aspx)**
 - **[Dokumentace Waveshare Current/Power Monitor HAT](https://www.waveshare.com/wiki/Current/Power_Monitor_HAT)**
