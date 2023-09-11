## LaskaKit Interaktivní Mapa ČR a srážkový radar ČHMÚ 
Tentokrát proměníme základní desku od LaskaKitu v podobě mapy ČR se 72 adresovatelnými RGB LED a SoC modulem ESP32-WROOM-32 ve srážkový radar. Skript v Pythonu na PC/Raspberry Pi/v cloudu nejprve stáhne a analyzuje bitmapu srážkového radaru ČHMÚ a při shodě geografických souřadnic odešle do ESP32 JSON s instrukcemi, které RGB LED deštěm zasažených měst rozsvítit a jakou barvou. 

Ve složce **arduino/ledmapa_klient** je verze firmwaru, který vedle HTTP serveru každých 10 minut automaticky stahuje nová radarová data pomocí **JSON API**. API je dostupné skrze HTTPS i HTTP a aktualizuje data každých 10 minut vždy 5. minutu (5, 15, 25, 35...)

 - **[Článek na Živě.cz](https://www.zive.cz/clanky/naprogramovali-jsme-radarovou-mapu-ceska-ukaze-kde-prave-prsi-a-muzete-si-ji-dat-na-zed/sc-3-a-222111/default.aspx)**
 - **[LaskaKit Interaktivní Mapa ČR](https://www.laskakit.cz/laskakit-interaktivni-mapa-cr-ws2812b/)**
 - **[LaskaKit Interaktivní Mapa ČR (GitHub)](https://github.com/LaskaKit/LED_Czech_Map)**
 - **[Srážkový radar ČHMÚ](https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/short.html)**

**JSON API s aktuálním stavem:**
 
 K dispozici je JSON, který vrací pouze seznam měst, kde prší přesně na jejich bodové GPS pozici, a JSON, který pokrývá širší oblast okolo středu.
 - **[JSON API (přesná shoda polohy města)](https://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=posledni.json)**
 - **[JSON API (shoda s širším čtvercem okolo města)](https://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=posledni_v2.json)**

**Bodové vs. areálové měření shody:**

V adresáři **[BodovyArealovyDetektor](https://github.com/jakubcizek/pojdmeprogramovatelektroniku/tree/master/SrazkovyRadar/BodovyArealovyDetektor)** se nachází komplentí příklad v Python upro spuštění HTTP serveru pomocí knihovny Tornado, který bude každých deset minut kontrolovat stav a vytvářet chace soubory s JSON pro bodové i areálové měření. V adresáři najdete také upravený příklad pro Arduino, který stahuje JSON z vlastního serveru, anbo z mého webového API.

Bodovému a areálovému měření se více věnuji **v časopisu Computer, vydání 10/2023 (říjen)**. Více zde: [ikiosek.cz/computer](https://www.ikiosek.cz/computer).
