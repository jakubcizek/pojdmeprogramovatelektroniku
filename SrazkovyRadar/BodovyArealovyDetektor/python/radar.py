from PIL import Image
from PIL import ImageDraw
import requests
from io import BytesIO
import json
from datetime import datetime, timedelta
import sys
from time import sleep
import colorsys

# Maji se vypisovat informace, kreslit kontrolni obrazky a ukladat JSON do textovych souboru?
logovani = True
kresleni = True
ukladani = True

# Zemepisne souradnice okraju radaroveho snimku
lon0 = 11.2673442
lat0 = 52.1670717
lon1 = 20.7703153
lat1 = 48.1

# Rozsah arealu pro plosnou analyzu
rozsah_px = 7

def printl(txt):
    if logovani:
        print(txt, flush=True)


def rgb_text(r,g,b, text):
    return f"\x1b[38;2;{r};{g};{b}m{text}\x1b[0m"


# Funkce pro stazeni radarovych dat. Pokud nedodame datum, funkce pouzije to aktualni. Pokud snimek jeste nebude existovat,
# zkusim o 10 minut starsi datum, nejvice ale tolikrat, kolik stanovuje promenna pokusy (5 ve vychozim stavu) 
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

# Funkce pro analyzu pixelu stazeneho snimku
def analyzuj_radar():
    sirka_stupne = lon1 - lon0
    vyska_stupne = lat0 - lat1

    ok, bajty, txt_datum = stahni_radar()
    if not ok:
        printl("Nepodařilo se stáhnout radarová data, končím :-(")
    else:
        try:
            radar = Image.open(BytesIO(bajty))
        except:
            printl("Nepodařilo se načíst bitmapu srážkového radaru")
            sys.exit(1)

        printl("Převádím snímek na RGB... ")
        radar = radar.convert("RGB")
   
        if kresleni:
            bitmapa_body = radar.copy()
            bitmapa_arealy = radar.copy()
            platno_body = ImageDraw.Draw(bitmapa_body)
            platno_arealy = ImageDraw.Draw(bitmapa_arealy)

        velikost_lat_pixelu = vyska_stupne / radar.height
        velikost_lon_pixelu = sirka_stupne / radar.width

        printl(f"Šířka obrázku: {radar.width} px ({sirka_stupne} stupňů zeměpisné délky)")
        printl(f"Výška obrázku: {radar.height} px ({vyska_stupne} stupňů zeměpisné šířky)")
        printl(f"1 vertikální pixel je roven {velikost_lat_pixelu} stupňům zeměpisné šířky")
        printl(f"1 horizontální pixel je roven {velikost_lon_pixelu} stupňům zeměpisné délky")

        body_s_destem = []
        arealy_s_destem = []
        mesta = {}

        # PRojdeme mesto po meste a porovname pixely
        printl("\nNačítám databázi měst... ")
        with open("mesta.csv", "r") as fi:
            mesta = fi.readlines()
            printl("Analyzuji, jestli v nich prší...")
            printl("-" * 80)

            for mesto in mesta:
                bunky = mesto.split(";")
                if len(bunky) == 4:
                    idx = int(bunky[0])
                    nazev = bunky[1]
                    lat = float(bunky[2])
                    lon = float(bunky[3])

                    mesta[idx] = {"nazev": nazev, "lat": lat, "lon": lon}
      
                    x = int((lon - lon0) / velikost_lon_pixelu)
                    y = int((lat0 - lat) / velikost_lat_pixelu)

                    # Projdeme ctverec arealu a vyhledame v nem ten nejvetsi dest
                    # podle odstinu od modre po rudou (pomuze barevny model HSV a slozka HUE)
                    hue = 0.8333 # 300 stupnu
                    r_areal, g_areal, b_areal = 0,0,0
                    for _x in range(x-rozsah_px, x + rozsah_px):
                        for _y in range(y-rozsah_px, y+rozsah_px):
                            _r, _g, _b = radar.getpixel((_x, _y))
                            if(_r + _g + _b) > 0:
                                h, s, v = colorsys.rgb_to_hsv(_r, _g, _b)
                                if h < hue:
                                    r_areal,g_areal,b_areal = _r, _g, _b
                                    hue = h
          
                    r, g, b = radar.getpixel((x, y))
      
                    if r + g + b > 0:
                        if kresleni:
                            platno_body.rectangle((x - 2, y - 2, x + 2, y + 2), fill=(r, g, b), outline=(255, 0, 0))
                        printl(f"💦  Ve městě {nazev} ({idx}) asi právě prší {rgb_text(r, g, b, f'(R={r} G={g} B={b})')}")
                        body_s_destem.append({"id": idx, "r": r, "g": g, "b": b})
                    else:
                            if kresleni:
                                platno_body.rectangle((x - 2, y - 2, x + 2, y + 2), fill=(0, 0, 0), outline=(255, 255, 255))

                    if r_areal+g_areal+b_areal > 0:
                        if kresleni:
                            platno_arealy.rectangle((x-rozsah_px, y-rozsah_px, x+rozsah_px, y+rozsah_px), fill=(r_areal, g_areal, b_areal), outline=(255, 0, 0))
                        printl(f"💦  V regionu {rozsah_px} x {rozsah_px} okolo {nazev} ({idx}) asi právě prší {rgb_text(r_areal,g_areal,b_areal, f'(R={r_areal} G={g_areal} B={b_areal})')}")
                        arealy_s_destem.append({"id": idx, "r": r_areal, "g": g_areal, "b": b_areal})
                    else:
                            if kresleni:
                                platno_arealy.rectangle((x-rozsah_px, y-rozsah_px, x+rozsah_px, y+rozsah_px), fill=(0, 0, 0), outline=(255, 255, 255))
        
        # Pokud je aktivni ukladani, ulozime JSON jako dva textove soubory
        if ukladani == True:
                archiv_body = {
                    "utc_datum":{
                        "rok": int(txt_datum[:4]),
                        "mesic": int(txt_datum[4:6]),
                        "den": int(txt_datum[6:8]),
                        "hodina": int(txt_datum[9:11]),
                        "minuta": int(txt_datum[11:13])
                    },
                    "seznam": body_s_destem
                }
                with open("cache/radar_body.json", "w") as soubor:
                    soubor.write(json.dumps(archiv_body))

                archiv_arealy = {
                    "utc_datum":{
                        "rok": int(txt_datum[:4]),
                        "mesic": int(txt_datum[4:6]),
                        "den": int(txt_datum[6:8]),
                        "hodina": int(txt_datum[9:11]),
                        "minuta": int(txt_datum[11:13])
                    },
                    "seznam": arealy_s_destem
                }
                with open("cache/radar_arealy.json", "w") as soubor:
                    soubor.write(json.dumps(archiv_arealy))

        # Pokud je aktivni kresleni, ulozime srovnavaci obrazky situace jako dva PNG
        if kresleni:
            bitmapa_body.save(f"obrazky/radar_body_{txt_datum}.png")
            bitmapa_arealy.save(f"obrazky/radar_arealy_{txt_datum}.png")

        
        return body_s_destem, arealy_s_destem, mesta

# Zacatek programu pri primem spusteni skriptu
if __name__ == "__main__":
    printl("*** RGB LED Srážkový radar ***\n")
    analyzuj_radar()
