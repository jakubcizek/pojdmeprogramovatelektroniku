## Vyhledávač nejbližšího deště 
Příloha článku v časopisu **Computer 11/2025**. Více zde: [ikiosek.cz/computer](https://www.ikiosek.cz/computer).

- **prsi.py**: Základní ukázka kódu, kestl iprší na určité XY pozici. Tento kód najdete včetně komentáře přímo v článku v časopisu Computer
- **server.py**: Kompletní server a JSON API napsaný v Python us využitím knihovny Tornado. Identický server běží na testovací adrese **[https://radar.kloboukuv.cloud](https://radar.kloboukuv.cloud)**

Testovací server běží na slabém virtuálním počítači v infrastruktuře Oracle Cloud. Výpočet nejbližšího deště tak může zabrat i okolo 70 ms. Na výkonném desktopovém počítači lze dosáhnout o řád kratšího času.
