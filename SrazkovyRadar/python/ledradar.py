from PIL import Image
from PIL import ImageDraw
import requests
from io import BytesIO
import json
from datetime import datetime, timedelta
import sys
from time import sleep

# -----------------------------------------------------------------------------
# GLOBALNI PROMENNE

logovani = True # Budeme vypisovat informace o běhu programu
kresleni = True # Uložíme snímek s radarem ve formátu radar_a_mesta_YYYYMMDD.HHM0.png
odesilani = True # Budeme odesílat data do LaskaKit mapy ČR

laskakit_mapa_url = sys.argv[1] # LAN IP/URL LaskaKit mapy ČR (bez http://) 

# Pracujeme v souřadnicovém systému WGS-84
# Abychom dokázali přepočítat stupně zeměpisné šířky a délky na pixely,
# musíme znát souřadnice levého horního a pravého dolního okraje radarového snímku ČHMÚ
# LEVÝ HORNÍ ROH
lon0 = 11.2673442
lat0 = 52.1670717
# PRAVÝ DOLNÍ ROH
lon1 = 20.7703153
lat1 = 48.1

# -----------------------------------------------------------------------------

# Alias pro print, který bude psát jen v případě,
# že má globální proměnná logovani hodnotu True
def printl(txt):
    if logovani:
        print(txt, flush=True)

# Funkce pro stažení bitmapy s radarovými daty z URL adresy:
# https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/data/czrad-z_max3d_masked/pacz2gmaps3.z_max3d.{datum_txt}.0.png
# datum_txt musí být ve formátu UTC YYYYMMDD.HHM0 (ČHMÚ zveřejňuje snímky každých celých 10 minut)
# Pokud URL není validní (obrázek ještě neexistuje),
# pokusím se stáhnout bitmapu s o deset minut starší časovou značkou
# Počet opakování stanovuje proměnná pokusy 
def stahni_radar(datum=None, pokusy=5):
    if datum == None:
        datum = datetime.utcnow()

    while pokusy > 0:
        datum_txt = datum.strftime("%Y%m%d.%H%M")[:-1] + "0"
        url = f"https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/data/czrad-z_max3d_masked/pacz2gmaps3.z_max3d.{datum_txt}.0.png"
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

# Funkce pro obarvení textu v terminálu pomocí RGB
# Použijeme pro nápovědu, jakou barvu mají pixely radarových dat v daném městě
# Záleží na podpoře v příkazové řádce/shellu
# We Windows Terminalu a v současných linuxových grafických shellech to zpravidla funguje 
def rgb_text(r,g,b, text):
    return f"\x1b[38;2;{r};{g};{b}m{text}\x1b[0m"

# Začátek běhu programu
if __name__ == "__main__":
    printl("*** RGB LED Srážkový radar ***\n")
    # Rozměry bitmapy ve stupních
    sirka_stupne = lon1 - lon0
    vyska_stupne = lat0 - lat1

    # Pokusím se stáhnout bitmapu s radarovými daty
    # Pokud se to podaří, ok = True, bajty = HTTP data odpovědi (obrázku), txt_datum = YYYYMMDD.HHM0 staženého snímku
    ok, bajty, txt_datum = stahni_radar()
    if not ok:
        printl("Nepodařilo se stáhnout radarová data, končím :-(")
    else:
        # Z HTTP dat vytvoříme objekt bitmapy ve formátu PIL/Pillow
        try:
            bitmapa = Image.open(BytesIO(bajty))
        # Pokud se to nepodaří, ukončíme program s chybovým kódem
        except:
            printl("Nepodařilo se načíst bitmapu srážkového radaru")
            sys.exit(1)

        # Původní obrázek používá indexovanou paletu barev. To se sice může hodit,
        # pro jendoduchost příkladu ale převedeme snímek na plnotučné RGB
        printl("Převádím snímek na RGB... ")
        bitmapa = bitmapa.convert("RGB")
        if kresleni:
            platno = ImageDraw.Draw(bitmapa)

        # Z pixelového rozměru bitmapy spočítáme stupňovou velikost vertikálního
        # a horizontálního pixelu pro další přepočty mezi stupni a pixely
        velikost_lat_pixelu = vyska_stupne / bitmapa.height
        velikost_lon_pixelu = sirka_stupne / bitmapa.width

        printl(f"Šířka obrázku: {bitmapa.width} px ({sirka_stupne} stupňů zeměpisné délky)")
        printl(f"Výška obrázku: {bitmapa.height} px ({vyska_stupne} stupňů zeměpisné šířky)")
        printl(f"1 vertikální pixel je roven {velikost_lat_pixelu} stupňům zeměpisné šířky")
        printl(f"1 horizontální pixel je roven {velikost_lon_pixelu} stupňům zeměpisné délky")

        # V souboru mesta.csv máme po řádcích všechny obce na LaskaKit mapě
        # Řádek má formát: ID;název;zem. šířka;zem. délka
        # ID představuje pořadí RGB LED na LaskaKit mapě ČR
        mesta_s_destem = []
        printl("\nNačítám databázi měst... ")
        with open("mesta.csv", "r") as fi:
            mesta = fi.readlines()
            printl("Analyzuji, jestli v nich prší...")
            printl("-" * 80)
            # Projdeme město po po městě v seznamu
            for mesto in mesta:
                bunky = mesto.split(";")
                if len(bunky) == 4:
                    idx = int(bunky[0])
                    nazev = bunky[1]
                    lat = float(bunky[2])
                    lon = float(bunky[3])
                    # Spočítáme pixelové souřadnice města na radarovém snímku
                    x = int((lon - lon0) / velikost_lon_pixelu)
                    y = int((lat0 - lat) / velikost_lat_pixelu)
                    # Zjistíme RGB na dané souřadnici, tedy případnou barvu deště
                    r,g,b = bitmapa.getpixel((x, y))
                    # Pokud je v daném místě na radarovém snímnku nenulová barva, asi v něm prší
                    # Intenzitu deště určí konkrétní barva v rozsahu od světle modré přes zelenou, rudou až bílou
                    # Právě zde bychom tedy mohli detekovat i sílu deště, pro jednoduchost ukázky si ale vystačíme s prostou barvou 
                    if r+g+b > 0:
                        # Pokud jsme na začátku programu aktivovali kreslení,
                        # na plátno nakreslíme čtvereček s rozměry 10×10 px představující město
                        # Čtvereček bude mít barvu deště a červený obrys
                        if kresleni:
                            platno.rectangle((x-5, y-5, x+5, y+5), fill=(r, g, b), outline=(255, 0, 0))
                        # Pokud je aktivní logování, vypíšeme barevný text s údajem, že v daném městě prší,
                        # a přidáme město na seznam jako strukturu {"id":id, "r":r, "g":g, "b":b}  
                        printl(f"💦  Ve městě {nazev} ({idx}) asi právě prší {rgb_text(r,g,b, f'(R={r} G={g} B={b})')}")
                        mesta_s_destem.append({"id": idx, "r": r, "g": g, "b": b})
                    else:
                            # Pokud v daném městě neprší, nakreslíme v jeho souřadnicích prázdný čtvereček s bílým obrysem
                            if kresleni:
                                platno.rectangle((x-5, y-5, x+5, y+5), fill=(0, 0, 0), outline=(255, 255, 255))
        
        # Prošli jsme všechna města, takže se podíváme,
        # jestli máme v seznamu nějaká, ve kterých prší
        if len(mesta_s_destem) > 0:
            # Pokud jsme aktivovali odesílání dat do LaskaKit mapy ČR,
            # uložíme seznam měst, ve kterých pršelo, jako JSON pole struktur
            # a tento JSON poté skrze HTTP POST formulář s názvem proměnné "mesta"
            # odešleme do LaskaKit mapy ČR, na které běží jednoduchý HTTP server
            if odesilani == True:
                printl("\nPosílám JSON s městy do LaskaKit mapy ČR...")
                form_data = {"mesta": json.dumps(mesta_s_destem)}
                r = requests.post(f"http://{laskakit_mapa_url}/", data=form_data)
                if r.status_code == 200:
                    printl(r.text)
                else:
                    printl(f"HTTP {r.status_code}: Nemohu se spojit s LaskaKit mapou ČR na URL http://{laskakit_mapa_url}/")
        else:
            printl("Vypadá to, že v žádném městě neprší!")

        # Pokud je aktivní kreslení, na úplný závěr
        # uložíme PNG bitmapu radarového snímku s vyznačenými městy
        # do souboru radar_a_mesta_YYYYMMDD.HHM0.png
        if kresleni:
            bitmapa.save(f"radar_a_mesta_{txt_datum}.png")
