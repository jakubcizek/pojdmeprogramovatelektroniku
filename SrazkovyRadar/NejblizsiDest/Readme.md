## Vyhledávač nejbližšího deště 
Příloha článku v časopisu **Computer 11/2025**. Více zde: [ikiosek.cz/computer](https://www.ikiosek.cz/computer).

- **prsi.py**: Základní ukázka kódu, kestl iprší na určité XY pozici. Tento kód najdete včetně komentáře přímo v článku v časopisu Computer
- **server.py**: Kompletní server a JSON API napsaný v Python us využitím knihovny Tornado. Identický server běží na testovací adrese **[https://radar.kloboukuv.cloud](https://radar.kloboukuv.cloud)**

Testovací server běží na slabém virtuálním počítači v infrastruktuře Oracle Cloud. Výpočet nejbližšího deště tak může při větší zátěži zabrat nějaký čas.

## Aktualizace: 27. dubna 2026 ##
Zatímco v článku v časopisu Computer demonstruji vyhledávání nejbližších barevných pixelů pomocí čítankového (ale nepříliš rychlého) algoritmu BFS, nová verze používá NumPy a výrazně rychlejší vektorový a maskový přístup. Výsledkem je zrychlení o řád a používání veřejného serveru výše by už nemělo dělat problémy.
