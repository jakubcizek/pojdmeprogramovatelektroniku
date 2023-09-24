## Detektor deště v oblasti zájmu pomocí Pythonu, JS a HTML
Tentokrát si vyrobíme detektor deště v oblasti zájmu (obdélníku), který nakreslíme nad mapou pomocí HTML aplikace. Server v Pythonu bude každých deset minut analyzovat déšť v oblasti zájmu na radarových snímcích ČHMÚ. Data bude ukládat do **historie.csv** a budou dostupná i skrze HTTP GET API n adrese **/?chcu=stav**. 

Skript **server.py** po spuštění nastartuje HTTP server na standardním TCP portu 80, začne periodicky analyzovat radarové snímky a ukládat stav v oblasti zájm udo souboru historie.csv
Projekt potřebuje k běhu vedle Pythonu 3.x ještě knihovny **[Tornado](https://www.tornadoweb.org/en/stable/)**, **[Schedule](https://schedule.readthedocs.io/en/stable/)** a **[Pillow](https://pillow.readthedocs.io/en/stable/)** 

Samotný detektor **radar.py** lze spouštět i samostatně. V takovém případě pro kontrolu vypíše JSON s deštěm v oblasti, která je definovaná v souboru **nastaveni_radaru.json**.

 - **[Článek na Živě.cz](https://www.zive.cz/clanky/naprogramovali-jsme-radarovou-mapu-ceska-ukaze-kde-prave-prsi-a-muzete-si-ji-dat-na-zed/sc-3-a-222111/default.aspx)**
 - **[Srážkový radar ČHMÚ](https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/short.html)**

**Podívejte se také na projekt s mapou ČR od LaskaKitu**
Na odkazu níže si pohrajeme s tištěnou mapou Česka se 72 RGB ELD, na kterých zobrazíme déšť v jendotlivých městech republiky.
**[Srážkový radar na mapě Česka od LaskaKitu](https://github.com/jakubcizek/pojdmeprogramovatelektroniku/tree/master/SrazkovyRadar)** 

