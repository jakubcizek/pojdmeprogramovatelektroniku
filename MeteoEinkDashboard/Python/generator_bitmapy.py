# Knihovna Pillow pro práci s rastrovými daty
# https://pillow.readthedocs.io/en/stable/
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont
# Knihovna Requests pro HTTP komunikaci
# https://requests.readthedocs.io/en/latest/
import requests
# Knihovna Pytz pro práci s časovými pásmy
# https://pytz.sourceforge.net/
import pytz
# Ostatní pomocné knihovny, kterou jso usoučástí instalace Pythonu
from io import BytesIO
from datetime import datetime, timedelta
import sys
from time import sleep
import struct

# Pokud True, budu do výstupu psát logovací informace
logovani = True

# Absolutn icesta k souborum v pripade spousteni skriptu z jineho adresare
cesta = ""

# Sedmibarevná paleta pro displeje E-ink ACeP/Gallery Palette
# Použijeme ji pro vytvoření bitmapy pro náš 7,3" 800x480 e-ink displej GDEY073D46 https://www.good-display.com/product/442.html
paleta = [
    [  0,   0,   0], # CERNA,    eink kod: 0x0
    [255, 255, 255], # BILA,     eink kod: 0x1
    [  0, 255,   0], # ZELENA,   eink kod: 0x2
    [  0,   0, 255], # MODRA,    eink kod: 0x3
    [255,   0,   0], # CERVENA,  eink kod: 0x4
    [255, 255,   0], # ZLUTA,    eink kod: 0x5
    [255, 128,   0]  # ORANZOVA, eink kod: 0x6
]

# Naivní funkce pro převod RGB odstínu na nejpodobnější/nejbližší barvu sedmibarevné palety
def prevedPixel(r, g, b):
    nejmensi_rozdil = 100e6
    nova_barva = 0
    for i, barva in enumerate(paleta):
        rozdil_r = r - barva[0]
        rozdil_g = g - barva[1]
        rozdil_b = b - barva[2]
        rozdil = (rozdil_r**2) + (rozdil_g**2) + (rozdil_b**2)
        if(rozdil < nejmensi_rozdil):
            nejmensi_rozdil = rozdil
            nova_barva =  i
    return nova_barva

# Funkce pro okamžité psaní do výstupu, pokud je povioleno logování
def printl(txt):
    if logovani:
        print(txt, flush=True)


# Funkce pro stažení teplotní heatmapy z webu In-pocasi.cz
# Teplotní snímky se generují každých celých třicet minut
# :00, :30  místního času (SEČ/SELČ)
def stahni_snimek_heatmapa(datum=None, pokusy=5):
    if datum == None:
        datum = datetime.now()

    while pokusy > 0:
        datum = datum.replace(minute=(datum.minute // 30) * 30)
        datum_txt = datum.strftime("%H%M")
        url = f"https://www.in-pocasi.cz/data/teplotni_mapy_cz_actual/t{datum_txt}.png"           
        printl(f"Stahuji soubor: {url}")
        r = requests.get(url)
        if r.status_code != 200:
            printl(f"HTTP {r.status_code}: Nemohu stáhnout soubor")
            printl("Pokusím se stáhnout o 30 minut starší soubor")
            datum -= timedelta(minutes=30)
            pokusy -= 1
            sleep(.5)
        else:
            return True, r.content, datum
    return False, None, datum

# Funkce pro stažení radarového snímky z webu ČHMÚ
# Radarové snímky se generují každých celých deset minut
# :00, :10, :20, ... UTC času
def stahni_snimek_radar(datum=None, pokusy=5):
    if datum == None:
        datum = datetime.utcnow()

    while pokusy > 0:
        # Získáme UTC datum a čas ve formátu YYYYmmdd.HHMM pro posledních celých deset minut
        datum = datum.replace(minute=(datum.minute // 10) * 10)
        datum_txt = datum.strftime("%Y%m%d.%H%M")
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
            return True, r.content, datum
    return False, None, datum

# Funkce pro stažení družicového snímku z webu ČHMÚ
# Družicové snímky se generují každou celou čtvrthodinu
# :00, :15, :30, :45 UTC času
def stahni_snimek_druzice(datum=None, pokusy=5):    
    if datum == None:
        datum = datetime.utcnow()

    while pokusy > 0:
        # Získáme UTC datum a čas ve formátu YYYYmmdd.HHMM pro poslední celou čtvrthodinu
        datum = datum.replace(minute=(datum.minute // 15 * 15))
        datum_txt = datum.strftime("%Y%m%d.%H%M")
        url = f"https://www.chmi.cz/files/portal/docs/meteo/sat/msg_hrit/img-msgcz-1160x800-ir108/msgcz-1160x800.ir108.{datum_txt}.0.jpg"
        printl(f"Stahuji soubor: {url}")
        r = requests.get(url)
        if r.status_code != 200:
            printl(f"HTTP {r.status_code}: Nemohu stáhnout soubor")
            printl("Pokusím se stáhnout o 15 minut starší soubor")
            datum -= timedelta(minutes=15)
            pokusy -= 1
            sleep(.5)
        else:
            return True, r.content, datum
    return False, None, datum


def rgb_text(r,g,b, text):
    return f"\x1b[38;2;{r};{g};{b}m{text}\x1b[0m"

if __name__ == "__main__":
    printl("*** Dashboard pro eink ***\n")

    # Stáhneme snímky radaru, družice a teplotní mapy
    # Snímky radaru ČHMÚ jsou k dispozici pod svobodnou licencí CC
    # Snímky z družice jsou k dispozici pod licencí EUMETSAT (https://www.chmi.cz/files/portal/docs/meteo/sat/info/EUM_licence.html), používáme v dobré víře pro edukační účely/soukromé použití
    # Snímky teplotní heatmapy stahujeme z webu In-pocasi.cz opět výhradně pro demonstrační a edukační účely a pro soukromé použití. Data náleží společnosti InMeteo, s.r.o.
    ok_radar, bajty_radar, datum_radar = stahni_snimek_radar()
    ok_heatmapa, bajty_heatmapa, datum_heatmapa = stahni_snimek_heatmapa()
    ok_druzice, bajty_druzice, datum_druzice = stahni_snimek_druzice()
    # Pokud se některé snímyk nestáhly, ukončíme skript,
    # protože nemáme kompletní fdata ke konstrukci dashboardu pro e-ink
    if not ok_radar or not ok_heatmapa or not ok_druzice:
        printl("Nepodařilo se stáhnout snimky, končím :-(")
    # V opačném případě...
    else:
        try:
            # Načteme stažené snímky
            snimek_radar = Image.open(BytesIO(bajty_radar))
            snimek_heatmapa = Image.open(BytesIO(bajty_heatmapa))
            snimek_druzice = Image.open(BytesIO(bajty_druzice))
        except:
            printl("Nepodařilo se načíst bitmapy")
            sys.exit(1)

        # Některé stažené snímky jsou v indexovaných barvách,
        # a tak všechn ysjeednotíme převodem na formát RGB
        printl("Převádím snímky na RGB...")
        snimek_radar = snimek_radar.convert("RGB")
        snimek_heatmapa = snimek_heatmapa.convert("RGB")
        snimek_druzice = snimek_druzice.convert("RGB")

        # Načtení předpřipraveného pokladu 800x480 px se statickými kresbami (ikony aj.)
        printl("Nahrávám podkladovou bitmapu...")
        podklad = Image.open(f"{cesta}podklad.png")
        podklad.convert("RGB")

        # Zmenšení stažených snímků a načtení černobílých masek republiky
        printl("Zmenšuji snímky a načítám masky republiky...")
        snimek_radar = snimek_radar.resize((400,240))
        snimek_heatmapa = snimek_heatmapa.resize((400,240))
        snimek_druzice = snimek_druzice.resize((400,240))
        maska_cesko_druzice = Image.open(f"{cesta}maska_cesko_druzice.png")
        maska_cesko_druzice = maska_cesko_druzice.convert("RGB")
        maska_cesko_radar = Image.open(f"{cesta}maska_cesko_radar.png")
        maska_cesko_radar = maska_cesko_radar.convert("RGB")

        # ---------------------------------
        # KRESBA BLOKU S TEPLOTNÍM SNÍMKEM
        # ---------------------------------
        printl("Kreslím heatmapu...")
        podklad.paste(snimek_heatmapa, (0, 0))

        # ---------------------------------
        # KRESBA BLOKU S DRUŽICOVÝM SNÍMKEM
        # ---------------------------------
        printl("Kreslím družicový snímek v umělých barvách a s maskou republiky...")
        # Protože budeme zjišťovat a modifikovat stav jendotlivých pixelů, projdeme je jeden p odruhým
        # Pomalý, ale jednoduchý/naivní postup
        for y in range(snimek_druzice.height):
            for x in range(snimek_druzice.width):
                r,g,b  = snimek_druzice.getpixel((x,y))
                _r,_g,_b  = maska_cesko_druzice.getpixel((x,y))
                # Pokud je pixel masky černý, nakresli pixel masky
                if _r == 0:
                    podklad.putpixel((x+400, y+240), (1, 1, 1))
                # V opačném případě kreslíme oblačnost v umělých barvách od žluté po modrou
                # Nemáme e-ink s odstíny šedi, takže nemůžeme použít originální barvy
                # Tímto způsobem můžeme dofiltrovat nejslabší oblačnost a kreslit jen tu zajímavou
                else:
                    if r >= 70 and r <= 80: # Nejslabsi mrak
                        podklad.putpixel((x+400, y+240), (255, 255, 0))
                    elif r > 80 and r <= 90: # Slaby mrak
                        podklad.putpixel((x+400, y+240), (255, 128, 0))
                    elif r > 90 and r <= 100: # Stredni mrak
                        podklad.putpixel((x+400, y+240), (0, 255, 0))
                    elif r > 100: # Silny mrak
                        podklad.putpixel((x+400, y+240), (0, 0, 255))
                    else:
                        podklad.putpixel((x+400, y+240), (255, 255, 255))

        # --------------------------------
        # KRESBA BLOKU S RADAROVÝM SNÍMKEM
        # --------------------------------
        printl("Kreslím radarový snímek a s maskou republiky...")
        # Protože budeme zjišťovat a modifikovat stav jendotlivých pixelů, projdeme je jeden p odruhým
        # Pomalý, ale jednoduchý/naivní postup
        for y in range(snimek_radar.height):
            for x in range(snimek_radar.width):
                r,g,b  = snimek_radar.getpixel((x,y))
                _r,_g,_b  = maska_cesko_radar.getpixel((x,y))
                # Pokud je pixel masky černý, nakresli pixel masky
                if _r == 0:
                        podklad.putpixel((x, y+240), (1, 1, 1))
                # V opačném případě vykreslíme radarová data
                # Ignoroujeme hodnotu kanálu 0 (černá/transparentní/pozadí)
                else:
                    if (r+g+b) > 0:
                        podklad.putpixel((x, y+240), (r, g, b))
            

        printl("Kreslím popisky...")
        platno = ImageDraw.Draw(podklad)
        pismo_mapy = ImageFont.truetype(f"{cesta}BakbakOne-Regular.ttf", 20)

        # Popisky teplotní mapy
        platno.text((10,10), "TEPLOTA", font=pismo_mapy, fill="black")
        platno.text((10,30), datum_heatmapa.strftime("%H:%M"), font=pismo_mapy, fill="black")

        # Popisky radarové mapy
        # Datum radarové mapy je v UTC, takže převedeme na časové pádsmo Česka
        casova_zona_cr = pytz.timezone("Europe/Prague")
        datum_radar = datum_radar.replace(tzinfo=pytz.utc).astimezone(casova_zona_cr)
        platno.text((10,250), "SRÁŽKY", font=pismo_mapy, fill="black")
        platno.text((10,270), datum_radar.strftime("%H:%M"), font=pismo_mapy, fill="black")

        # Popisky družicové mapy
        # Datum družicového snímku je také v UTC, takže opět převedeme na čas Česka
        datum_druzice = datum_druzice.replace(tzinfo=pytz.utc).astimezone(casova_zona_cr)
        platno.text((410,250), "DRUŽICE", font=pismo_mapy, fill="black")
        platno.text((410,270), datum_druzice.strftime("%H:%M"), font=pismo_mapy, fill="black")

       # Nakreslení dat z meteostanice
        printl("Zjišťuji stav redakční meteostanice...")
        fi = open("/www/solar2.txt", "r")
        lines = fi.readlines()
        fi.close()
        last = lines[-1]
        chops = last.split(";")
        teplota = chops[1]
        vlhkost = chops[2]
        svetlo = chops[3]
        baterie = chops[4]

        printl("Kreslím údaje z meteostanice...")
        pismo_meteo_datum = ImageFont.truetype(f"{cesta}BakbakOne-Regular.ttf", 80)
        pismo_meteo_data = ImageFont.truetype(f"{cesta}BakbakOne-Regular.ttf", 35)
        pismo_meteo_data_mensi = ImageFont.truetype(f"{cesta}BakbakOne-Regular.ttf", 25)
        platno.text((480, 90), f"{teplota.replace('.',',')} °C", font=pismo_meteo_data, fill="black")
        platno.text((670, 90), f"{vlhkost.replace('.',',')} %", font=pismo_meteo_data, fill="black")
        platno.text((480, 177), f"{svetlo.replace('.',',')} lx", font=pismo_meteo_data_mensi, fill="black")
        platno.text((670, 166), f"{baterie.replace('.',',')} V", font=pismo_meteo_data, fill="black")
        dny_v_tydnu = ['Po', 'Út', 'St', 'Čt', 'Pá', 'So', 'Ne']
        datum = datetime.now()
        platno.text((425, -20), f"{datum.strftime('%d.%m.')} {dny_v_tydnu[datum.weekday()]}", font=pismo_meteo_datum, fill="black")

        # Ještě nakreslíme mřížku
        platno.line([(400,0),(400,480)], fill="black", width=1)
        platno.line([(0,240),(800,240)], fill="black", width=1)


        # Převod bitmapy na sedmibarevnou paletu ACeP/Gallery Palette 
        printl("Převádím RGB na sedmibarevnou paletu E-ink Gallery Palette...")
        printl("Provádím zákaldní bezztrátovou kompresi RLE...")
        binarni = open(ff"{cesta}dashboard_rle.bin", "wb")
        bajty = []
        for y in range(podklad.height):
            for x in range((int(podklad.width/2))):
                px1 = podklad.getpixel(((x * 2), y))
                px2 = podklad.getpixel(((x * 2) + 1, y))
                barva1 = prevedPixel(px1[0], px1[1], px1[2])
                barva2 = prevedPixel(px2[0], px2[1], px2[2])
                par = barva2 | (barva1 << 4) 
                bajty.append(par)
        komprimovano = []
        hodnota = bajty[0]
        pocet = 0
        # Bezztrátová komprese základním algoritmem RLE s osmibitovou délkou
        # Naivní a pomalý přístup pro demosntraci a pochopení
        for bajt in bajty:
            if bajt != hodnota:
                komprimovano.append(pocet)
                komprimovano.append(hodnota)
                hodnota = bajt
                pocet = 1
            else:
                if pocet == 255:
                    komprimovano.append(pocet)
                    komprimovano.append(hodnota)
                    hodnota = bajt
                    pocet = 1
                else:
                    pocet += 1
        printl(f"Komprimováno: {len(komprimovano)} bajtů")
        printl("Ukládám...")
        # Uložíme zkomprimovanou bitmapu do souboru
        for bajt in komprimovano:
            binarni.write(struct.pack("B", bajt))
        binarni.close()
