# Knihovna stromečku
# https://github.com/ThePiHut/rgbxmastree
# Z GitHubu je třeba stáhnout soubor tree.py a umístit jej do stejného adresáře
# Stromeček vyžaduje knihovnu gpiozero pro Raspberry Pi
# Měla být součástí oficiální distribuce Raspberry Pi OS, anebo ji doinstalujte ručně:
# sudo apt install python3-gpiozero
from tree import RGBXmasTree

# Práce s barevným modelem Hue
from colorzero import Hue

# Práce s náhodnými čísly
import random

# Zjištění přidělené LAN IP adresy
import os
import re

# Ukončení programu stiskem CTRL+C
import signal

# PRáce s vlákny
from threading import Thread

# Práce s HTTP serverem Tornado
from tornado import ioloop
from tornado import web

# Základní barvy RGB
# Rozsah každého kanálu v rozmezí 0-1 (obdoba 0-255)
zakladniBarvy = [
    (1, 0, 0), # červená
    (0, 1, 0), # zelená
    (0, 0, 1), # modrá
    (1, 1, 0), # žlutá
    (1, 0, 1), # purpurová
    (0, 1, 1), # tyrkysová
    (1, 1, 1)  # bílá
]

# Po příchodu systémové zprávy SIGINT
# bezpečně ukonči program
def sigint(sig, frame):
    global animacniSmyckaBezi
    print("Ukončuji program...")
    animacniSmyckaBezi = False
    if hlavniSmycka is not None:
        print("Vypínám hlavní smyčku")
        hlavniSmycka.stop()
    print("Vypínám stromeček")
    stromecek.color = (0, 0, 0)
    stromecek.close()
    exit()

# Funkce pro získání IP adresy podle zadaného rozhraní
# Pro Wi-Fi se přečte odpověď systémového příkazu: ip a show wlan0 | grep inet 
def ziskejIP(rozhrani):
    radek = os.popen("ip a show " + rozhrani + "| grep inet").read().split("\n")[0].strip()
    if not rozhrani in radek:
        return None
    else:
        adresy = re.findall(r"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})", radek)
        return adresy[0]

# Analaogie funkce map ze světa Arduino
# Přepočítá hodnotu x podle vstupního rozsahu in na cílový rozsah out
def map(x, in_min, in_max, out_min, out_max):
    return int((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)

# Animační smyčka, která se spustí v samostatném vlákně
def animacniSmycka():
    # Opakuje stále dokola, dokud má tato proměnná hodnotu True
    while animacniSmyckaBezi:
        # Náhodná animace
        # Nastaví náhodnou základní barvu na náhodné LED stromečku
        if animace == "náhodná":
            led = random.choice(stromecek)
            led.color = random.choice(zakladniBarvy)
        # Animace Hue
        # Postupně mění odstín v modelu Hue
        elif animace == "hue":
            stromecek.color += Hue(deg=1)

# Třída HttpIndex pro HTTP GET komunikaci
# Při zadání adresy / server vrátí soubor index.html
# Zároveň reaguje na URL parametry prikaz, led a rgb
class HttpIndex(web.RequestHandler):
    def get(self):
        global stromecek
        global animace
        prikaz = self.get_argument("prikaz", default="").lower()
        led = self.get_argument("led", default="").lower()
        rgbhex = self.get_argument("rgb", default="").lower()

        # Pokud je prikaz roven hodnotě rozsvítit,
        # rozsvítí vybranou LED zvolenou barvou.
        # URL parametr rgb musí být v HEX formátu RRGGBB (Třeba FF0000 = červená)
        if prikaz == "rozsvitit":
            animace = ""
            led = int(led)
            r = int(rgbhex[0:2], 16)
            g = int(rgbhex[2:4], 16)
            b = int(rgbhex[4:6], 16)

            r = map(r, 0, 255, 0, 100) / 100.0
            g = map(g, 0, 255, 0, 100) / 100.0
            b = map(b, 0, 255, 0, 100) / 100.0

            if led < 0:
                stromecek.color = (r, g, b)
                self.write(f"Nastavuji všechny LED na barvu #{rgbhex}")  
            else:
                stromecek[led].color = (r, g, b)  
                self.write(f"Nastavuji LED {led} na barvu #{rgbhex}")    

        # Pokud se má spustit animace, nastav její proměnnou, 
        # kterou stále dokola kontroluje v separátním vlákně funkce animacniSmycka
        elif "animuj" in prikaz:
            if prikaz == "animujnahodne":
                animace = "náhodná"

            elif prikaz == "animujhue":
                animace = ""
                stromecek.color = (1, 0, 0)
                animace = "hue"
            else:
                animace = ""
                self.write(f"Neznámá animace")
        
            self.write(f"Zapínám animaci {animace}") 

        # Pokud se ají všechn yLED zhasnout, nastav marvu celého stromečku na 0, 0, 0
        elif prikaz == "zhasni":
            animace = ""
            stromecek.color = (0, 0, 0)
            self.write(f"Zhasínám stromeček") 

        # Ve všech ostatních případech vrať HTTP klientu obah souboru index.html
        else:
            self.render("index.html")


# TADY ZAČÍNÁ BĚH NAŠEHO PROGRAMU
if __name__== "__main__":
    print("Startuji Stromeček 1.0")

    # Nastartujeme stromeček s nízkým jasem 0.1 (0 až 1)
    # Ve výchozí mstavu bude zhasnutý, nastavíme tedy RGB barvu 0, 0, 0
    stromecek = RGBXmasTree()
    stromecek.brightness = .1
    stromecek.color = (0, 0, 0)

    # Aktivujeme smyčku v separátním vlákně
    animace = ""
    animacniSmyckaBezi = True
    
    # A separátní vlákno nastartujeme
    animacniVlakno = Thread(target = animacniSmycka) 
    animacniVlakno.deamon = True  
    animacniVlakno.start()


    # Získáme IP adresu Raspberry Pi Zero 2 W připojeného k síti skrze Wi-Fi
    ip = ziskejIP("wlan0")

    # Spustíme funkci sigint po příchodu systémového přerušení SIGINT
    signal.signal(signal.SIGINT, sigint)

    # Zaregistrujeme HTTP GET požadavek /, který zpracuje třída HttpIndex
    print(f"Startuji HTTP server na adrese {ip}:80")
    webserver = web.Application([
        (r'/', HttpIndex)
    ])
    # Nastartujeme HTTP server na TCP portu 80
    webserver.listen(80)

    # Nastartujeme hlavní smyčku třídy Tornado, která bude vše řídit
    print("Startuji hlavní smyčku, pomodleme se!")
    print("Pro ukončení stiskněte CTRL+C")
    hlavniSmycka = ioloop.IOLoop.current()
    hlavniSmycka.start()

    # Nyní je program spuštěný a bude spuštěný tak dlouho,
    # dokud nestiskneme kombinaci CTRL+C, anebo jej zabijeme jiným způsobem
