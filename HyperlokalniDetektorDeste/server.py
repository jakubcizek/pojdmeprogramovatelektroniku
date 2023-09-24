# Ke spuštění HTTP serveru potřebujeme nainstalovat knihovnu Tornado
# https://www.tornadoweb.org/en/stable/
import tornado.ioloop
import tornado.web
import json

# Pro přesné plánování úloh potřebujeme nainstalovat knihovnu Schedule
# https://schedule.readthedocs.io/en/stable/
# Knihovna bude stahovat nové snímky vždy v časech :03, :13, :23 atd.
# Každých celých deset minut (:00, :10 atd.) totiž ČHMÚ generuje nové snímky radaru
# V praxi jsou ale dostupné třeba o 1-2 minuty později, a tak chceme mít jistotu,
# že 3 minuty poté už určitě budou k dispozici
import schedule

# Skript dekodéru radarových dat radar.py musí být ve stejném adresáři jako skript serveru
import radar
nastaveni_radaru = {} # Objekt s nastavením radaru/oblasti zájmu
situace = {} # Objekt s posledním známým stavem radaru v oblasti zájmu

# Funkce pro nahrání konfigurace radaru ze souboru nastaveni_radaru.json
def nahraj_nastaveni_radaru():
    global nastaveni_radaru
    with open("nastaveni_radaru.json", "r") as soubor:
        nastaveni_radaru = json.loads(soubor.read())

# Funkce pro uložení nastavení radaru do souboru
def uloz_nastaveni_radaru():
    global nastaveni_radaru
    with open("nastaveni_radaru.json", "w") as soubor:
        soubor.write(json.dumps(nastaveni_radaru))

# Smyčka, která se volá každou sekundu
# Slouží knihovně Schedule pro aktualizování dat z radaru každých 10 minut
def smycka_presneho_planovace():
    schedule.run_pending()

# Funkce pro aktualizaci radaru
def aktualizuj_radar():
    global situace
    radar.kresleni = True # Pokud True, dekodér radaru bude do složky obrazky ukladat kontrolní snímky s vyznačenou oblastí zájmu
    radar.logovani = False # Pokud True, dekodér radaru bude vyspiovat informace za běhu (jen při samosttaném spuštění skriptu radar.py)
    data = radar.analyzuj_radar(nastaveni_radaru) # Zjisti stav radaru
    # Pokud se podařilo stáhnout a zpracovat data z radaru
    if data != None:
        situace = data
        # Přidej do historie.csv nový řádek s aktuálními hodnotami
        with open("historie.csv", "a") as soubor:
            radek = f"{situace['datum']};{situace['prsi']};{situace['analyza']}"
            if situace["analyza"] == "bodova":
                radek += f";{situace['skore']}\n"
            elif situace["analyza"] == "plosna":
                radek += f";{situace['pixely_s_destem']};{situace['plocha_procenta']:.2f};{situace['prumerne_skore']};{situace['nejvyssi_skore']['skore']}\n"
            soubor.write(radek)


# Třída HTTP pozadavku klienta
# Zpracuje se při HTTP požadavku na /
# Podle URL parametru /?chcu=xxx vrátime konkrétní data/JSON
class RadarServer(tornado.web.RequestHandler):
    # Pokud je to HTTP GET
    def get(self):
        global nastaveni_radaru
        chcu = self.get_argument("chcu", "")
        
        # Pokud chci znát stav radarových dat v oblasti zájmu
        if chcu == "stav":
            self.set_header("Content-Type", "application/json")
            self.write(json.dumps(situace))

        # Pokud chci nastavit novou oblast zájmu
        elif chcu == "nastavit-radar":
            nastaveni_radaru["zemepisne_souradnice"] = False
            nastaveni_radaru["souradnice"] = (
                int(self.get_argument("x0", "0")),
                int(self.get_argument("y0", "0")),
                int(self.get_argument("x1", "0")),
                int(self.get_argument("y1", "0")),
            )
            uloz_nastaveni_radaru()
            aktualizuj_radar()
            self.set_header("Content-Type", "application/json")
            self.write(json.dumps(situace))

        # Pokud chci znát rozměry oblasti zájmu
        elif chcu == "nastaveni-radaru":
            self.set_header("Content-Type", "application/json")
            self.write(json.dumps(nastaveni_radaru))

        # Pokud chci bitmpau s podkladovou mapou    
        elif chcu == "podkladova-mapa":
            with open("www/podkladova_mapa.jpg", "rb") as soubor:
                data = soubor.read()
                self.set_header("Content-Type", "image/jpeg")
                self.write(data)

        # Ve všech ostatních případech vrať klientovi HTML kód
        # stránky pro nastavení oblasti zájmu
        else:
            with open("www/mapa.html", "r") as soubor:
                data = soubor.read()
                self.set_header("Content-Type", "text/html; charset=utf-8")
                self.write(data)


# Začátek programu
if __name__ == "__main__":
    # NA úvod provedeme analýzu, aby byla hned k dispozici
    print("Na úvod analyzuji radar")
    nahraj_nastaveni_radaru()
    aktualizuj_radar()

    # Nastartujeme HTTP server na standardním TCP portu 80
    # Při HTTP pozadavku /... se zpracuje třída RadarServer
    print("Startuji HTTP server na standardnim TCP portu 80")
    web = tornado.web.Application([
        (r'/?.*', RadarServer)
    ])
    web.listen(80)

    # Pomocí knihovny Schedule naplánujeme aktualizaci radaru v přesných minutových časech
    schedule.every().hour.at(":03").do(aktualizuj_radar)
    schedule.every().hour.at(":13").do(aktualizuj_radar)
    schedule.every().hour.at(":23").do(aktualizuj_radar)
    schedule.every().hour.at(":33").do(aktualizuj_radar)
    schedule.every().hour.at(":43").do(aktualizuj_radar)
    schedule.every().hour.at(":53").do(aktualizuj_radar)

    # Pomoci základní aplikační (nekonečné) smyčky naplánujeme periodické
    # spouštění smyšky knihovny Schedule – ta totiž vlastní paralelní ssmyčku nemá
    # a právě tímto periodickým voláním si teprve zjišťuje aktuální čas
    uloha = tornado.ioloop.PeriodicCallback(smycka_presneho_planovace, 1000)
    uloha.start()

    # Nastartujeme hlavní apliakční (nekonečnou) smyčku
    # Můžeme z ní vyskočit CTRL+C, anebo ukončenim procesu jiným způsobem 
    print("Startuji aplikační smyčku...\nPro ukončení stiskněte CTRL+C, nebo zabijte proces")
    try:
        tornado.ioloop.IOLoop.current().start()
    except KeyboardInterrupt:
        print("Nashledanou a přijďte zas :-)")
