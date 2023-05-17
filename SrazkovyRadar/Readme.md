## LaskaKit Interaktivní Mapa ČR a srážkový radar ČHMÚ 
Tentokrát proměníme základní desku od LaskaKitu v podobě mapy ČR se 72 adresovatelnými RGB LED a SoC modulem ESP32-WROOM-32 ve srážkový radar. Skript v Pythonu na PC/Raspberry Pi/v cloudu nejprve stáhne a analyzuje bitmapu srážkového radaru ČHMÚ a při shodě geografických souřadnic odešle do ESP32 JSON s instrukcemi, které RGB LED deštěm zasažených měst rozsvítit a jakou barvou. 

Ve složce **arduino/ledmapa_klient** je verze firmwaru, který vedle HTTP serveru každých 10 minut automaticky stahuje nová radarová data pomocí **JSON API**. API je dostupné skrze HTTPS i HTTP a aktualizuje data každých 10 minut vždy 5. minutu (5, 15, 25, 35...)

 - **[Článek na Živě.cz](https://www.zive.cz/clanky/naprogramovali-jsme-radarovou-mapu-ceska-ukaze-kde-prave-prsi-a-muzete-si-ji-dat-na-zed/sc-3-a-222111/default.aspx)**
 - **[LaskaKit Interaktivní Mapa ČR](https://www.laskakit.cz/laskakit-interaktivni-mapa-cr-ws2812b/)**
 - **[LaskaKit Interaktivní Mapa ČR (GitHub)](https://github.com/LaskaKit/LED_Czech_Map)**
 - **[Srážkový radar ČHMÚ](https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/short.html)**
 - **[JSON API](https://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=posledni.json)**


