# Pro práci s bitmapou radaru potřebujeme nainstalovat knihovnu Pillow
# https://pillow.readthedocs.io/en/stable/
from PIL import Image
from PIL import ImageDraw
import requests
from io import BytesIO
import json
from datetime import datetime, timedelta
import sys
from time import sleep
import colorsys

# Mají se vypisovat informace a kreslit kontrolní obrázky?
logovani = True
kresleni = True

# Zeměpisné souřadnice okrajů radarového snímku
lon0 = 11.2673442
lat0 = 52.1670717
lon1 = 20.7703153
lat1 = 48.1

# Funcke pro výpis do standardního výstupu, pokud je povolené logování
def printl(txt):
    if logovani:
        print(txt, flush=True)


# Přemapování hodnoty z jednoho rozsahu do ddruhého
# Aanalogie funkce map() ze světa Arduino
def premapuj_rozsah(x, in_min, in_max, out_min, out_max):
    return int((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)

# Funcke pro výpočet skóre deště pomocí barevného model uHSV
def spocitej_skore(r, g, b):
    if r+g+b == 0:
        return 0
    h, s, v, = colorsys.rgb_to_hsv(r, g, b)
    # Pokud nám pixel svítí bíle, vrátím maximální intenzitu deště
    if s == 0 and v == 1:
        return 100
    else:
        # Převedeme hue na 0-360
        h *=  360
        # Zajímá nás jen rozsah 0-240 (červená až modrá)
        if h > 240:
            # Hue vyšší než 315 (růžová až červená z opačné strany) vrátíme jako 0 (červená)
            if h < 315:
                # Hue 241-314 (modrá až fialová) vrátíme jako 240 (modrá)
                h = 240
            else:
                h = 0
        # Přepočítáme rozsah 0-240 na 1-99 
        return premapuj_rozsah(h, 0, 240, 99, 1)


# Funkce pro stažení radarových dat. Pokud nedodáme datum, funkce použije to aktuální. Pokud snímek ještě nebude existovat,
# zkusíme o 10 minut starší datum, nejvíce ale tolikrát, kolik stanovuje proměnna pokusy (5 ve výchozím stavu) 
def stahni_radar(datum=None, pokusy=5):
    if datum == None:
        datum = datetime.utcnow()

    while pokusy > 0:
        datum_txt = datum.strftime("%Y%m%d.%H%M")[:-1] + "0"
        url = f"https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/data/czrad-z_max3d/pacz2gmaps3.z_max3d.{datum_txt}.0.png"           
        printl(f"Stahuji soubor: {url}")
        r = requests.get(url)
        if r.status_code != 200:
            printl(f"HTTP {r.status_code}: Nemohu stáhnout soubor")
            printl("Pokusím se stáhnout o 10 minut starší soubor")
            datum -= timedelta(minutes=10)
            pokusy -= 1
            sleep(.5)
        else:
            return True, r.content, datum_txt
    return False, None, datum_txt 

# Funkce pro analýzu pixelů staženého snímku
def analyzuj_radar(areal_zajmu):

    sirka_stupne = lon1 - lon0
    vyska_stupne = lat0 - lat1

    ok, bajty, txt_datum = stahni_radar()
    if not ok:
        printl("Nepodařilo se stáhnout radarová data, končím :-(")
        return None
    else:
        try:
            radar = Image.open(BytesIO(bajty))
        except:
            printl("Nepodařilo se načíst bitmapu srážkového radaru")
            sys.exit(1)

        printl("Převádím snímek na RGB... ")
        radar = radar.convert("RGB")
   
        if kresleni:
            platno = ImageDraw.Draw(radar)

        velikost_lat_pixelu = vyska_stupne / radar.height
        velikost_lon_pixelu = sirka_stupne / radar.width

        printl(f"Šířka obrázku: {radar.width} px ({sirka_stupne} stupňů zeměpisné délky)")
        printl(f"Výška obrázku: {radar.height} px ({vyska_stupne} stupňů zeměpisné šířky)")
        printl(f"1 vertikální pixel je roven {velikost_lat_pixelu} stupňům zeměpisné šířky")
        printl(f"1 horizontální pixel je roven {velikost_lon_pixelu} stupňům zeměpisné délky")

        # Pokud máme souřadnice oblasti zájmu v zeměpisných stupních
        if areal_zajmu["zemepisne_souradnice"]:
            printl("Převádím zeměpisné souřadnice na souřadnice radarvfoého snímku")
            # Pokud hledáme déšť v obdélniku
            if len(areal_zajmu["souradnice"]) == 4:
                # Levý horní okraj přepočítaný na pixely
                areal_zajmu["souradnice"][0] = int((areal_zajmu["souradnice"][0] - lon0) / velikost_lon_pixelu)
                areal_zajmu["souradnice"][1] = int((lat0 - areal_zajmu["souradnice"][1]) / velikost_lon_pixelu)
                # Pravý dolní okraj přepočítaný na pixely
                areal_zajmu["souradnice"][2] = int((areal_zajmu["souradnice"][2] - lon0) / velikost_lon_pixelu)
                areal_zajmu["souradnice"][3] = int((lat0 - areal_zajmu["souradnice"][3]) / velikost_lon_pixelu)
            # Pokud hledáme déšť v exaktním bodu
            elif len(areal_zajmu["souradnice"]) == 2:
                areal_zajmu["souradnice"][0] = int((areal_zajmu["souradnice"][0] - lon0) / velikost_lon_pixelu)
                areal_zajmu["souradnice"][1] = int((lat0 - areal_zajmu["souradnice"][1]) / velikost_lon_pixelu)

        printl(f"Hledám déšť na souřadnicích: {areal_zajmu['souradnice']}:")

        # Pokud se jedná o bod (2 souřadnice X,Y)
        if len(areal_zajmu["souradnice"]) == 2:
            printl("Provádím bodovou analýzu")
            r, g, b = radar.getpixel((areal_zajmu["souradnice"][0], areal_zajmu["souradnice"][1]))
            if r + g + b > 0:
                printl("Našel jsem déšť!")
                skore = spocitej_skore(r, g, b)
                return {
                    "prsi": True,
                    "analyza": "bodova",
                    "rgb": {"r": r, "g": g, "b": b},
                    "skore": skore,
                    "datum": datetime.now().strftime("%H:%M:%S, %d.%m.%Y")
                }
            else:
                printl("Nenašel jsem déšť")
                return {
                    "prsi": False,
                    "analyza": "bodova",
                    "rgb": {"r": 0, "g": 0, "b": 0},
                    "skore": 0,
                    "datum": datetime.now().strftime("%H:%M:%S, %d.%m.%Y")
                }

        # Pokud se jedná o obdélník (4 souřadnice X0,Y0,X1,Y1)    
        elif len(areal_zajmu["souradnice"]) == 4:
            printl("Provádím plošnou analýzu")
            pixely_s_destem = 0
            nejvyssi_skore = 0
            nejvyssi_skore_r = 0
            nejvyssi_skore_g = 0
            nejvyssi_skore_b = 0
            prumerne_skore = 0
            plocha_procenta = 0
            for x in range(areal_zajmu["souradnice"][0], areal_zajmu["souradnice"][2]):
                for y in range(areal_zajmu["souradnice"][1], areal_zajmu["souradnice"][3]):
                    r, g, b = radar.getpixel((x, y))
                    if r + g + b > 0:
                        pixely_s_destem += 1
                        skore = spocitej_skore(r, g, b)
                        prumerne_skore += skore
                        if skore > nejvyssi_skore:
                            nejvyssi_skore = skore
                            nejvyssi_skore_r = r
                            nejvyssi_skore_g = g
                            nejvyssi_skore_b = b
            
            if pixely_s_destem > 0:
                printl("Našel jsem déšt !")
                prumerne_skore = prumerne_skore / pixely_s_destem
                plocha_procenta = pixely_s_destem / (
                        (areal_zajmu["souradnice"][2] - areal_zajmu["souradnice"][0]) * (areal_zajmu["souradnice"][3] - areal_zajmu["souradnice"][1])
                        / 100.0
                )

                # Pokud je aktivní kreslení kontrolních obrázků
                if kresleni:
                    platno.rectangle(areal_zajmu["souradnice"], outline=(255, 0, 0))
                    for x in range(areal_zajmu["souradnice"][0], areal_zajmu["souradnice"][2]):
                        for y in range(areal_zajmu["souradnice"][1], areal_zajmu["souradnice"][3]):
                           r, g, b = radar.getpixel((x, y))
                           if(r == nejvyssi_skore_r and g == nejvyssi_skore_g and b == nejvyssi_skore_b):
                               printl("Kreslim nejvyssi pixel")
                               radar.putpixel((x, y), (255, 0, 0))
                    radar.save(f"obrazky/radar_{txt_datum}.png")

                return {
                    "prsi": True,
                    "analyza": "plosna",
                    "pixely_s_destem": pixely_s_destem,
                    "plocha_procenta": plocha_procenta,
                    "nejvyssi_skore": {
                        "skore": nejvyssi_skore,
                        "r": nejvyssi_skore_r,
                        "g": nejvyssi_skore_g,
                        "b": nejvyssi_skore_b,
                    },
                    "prumerne_skore": prumerne_skore,
                    "datum": datetime.now().strftime("%H:%M:%S, %d.%m.%Y")
                }
            else:
                printl("Nenašel jsem déšť")
                return {
                    "prsi": False,
                    "analyza": "plosna",
                    "pixely_s_destem": 0,
                    "plocha_procenta": 0,
                    "nejvyssi_skore": {
                        "skore": 0,
                        "r": 0,
                        "g": 0,
                        "b": 0,
                    },
                    "prumerne_skore": 0,
                    "datum": datetime.now().strftime("%H:%M:%S, %d.%m.%Y")
                }
            
# Funcke pro nahrání nastavení oblasti zájmu
def nahraj_nastaveni_radaru():
    with open("nastaveni_radaru.json", "r") as soubor:
        return json.loads(soubor.read())      

# Začátek programu při jeho samostatném spuštění
# Server jej bude používat jako knihovnu, čili tuto část ignoruje
if __name__ == "__main__":
    printl("*** Srážkový radar ***\n")

    nastaveni = nahraj_nastaveni_radaru()
    data = analyzuj_radar(nastaveni)
    if data == None:
        print("Nepodařilo se stáhnout snímek z radaru ČHMÚ")
    else:
        print(data)

    