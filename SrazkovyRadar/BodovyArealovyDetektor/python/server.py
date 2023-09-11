# Ke spusteni HTTP serveru potrebujeme knihovnu Tornado
# https://www.tornadoweb.org/en/stable/
import tornado.ioloop
import tornado.web

# Skript dekoderu radar.py ve stejnem adresari
import radar

# Trida HTTP pozadavku klienta
# Zpracuje se pri HTTP pozadavku na /
# Podle URL parametru /?mereni=bodove, nebo /?mereni=arealove
# vratime konkretni soubor JSON
class RadarServer(tornado.web.RequestHandler):
    # Pokud je to HTTP GET
    def get(self):
        mereni = self.get_argument("mereni", "bodove")
        if mereni == "bodove":
            self.render("cache/radar_body.json")
        elif mereni == "arealove":
            self.render("cache/radar_arealy.json")

# Funkce pro aktualizaci radaru
def aktualizuj_radar():
    radar.kresleni = False
    radar.logovani = False
    radar.ukladani = True
    radar.analyzuj_radar()


# Zacatek programu pri primem spusteni v Pythonu
if __name__ == "__main__":
    # Na uvod provedeme analyzu psoledniho snimku
    print("Na úvod analyzuji radar")
    radar.kresleni = False
    radar.logovani = False
    radar.ukladani = True
    body, arealy, mesta  = radar.analyzuj_radar()
    if len(body) > 0:
        print("V těchto přesných bodech prší:")
        for bod in body:
            print(" - " + mesta[bod["id"]]["nazev"])
    else:
        print("V žádném sledovaném bodě neprší")

    if len(arealy) > 0:
        print("V těchto širších areálech prší:")
        for areal in arealy:
            print(" - " + mesta[areal["id"]]["nazev"])
    else:
        print("V žádném sledovaném areálu neprší")

    # Nastartujeme HTTP server na standardnim TCP portu 80
    # Pri HTTP pozadavku /... se zpracuje trida RadarServer
    print("Startuji HTTP server na standardnim TCP portu 80")
    web = tornado.web.Application([
        (r'/?.*', RadarServer)
    ])
    web.listen(80)

    # Nastartujeme planovani ulohy, kdy se nam bude kazdych 600 sekund volat funkce aktualizuj_radar
    uloha = tornado.ioloop.PeriodicCallback(aktualizuj_radar, 60000)
    uloha.start()

    # Nastartujeme hlavni aplikacni nekonecnou smycku
    # Muzeme z ni vyskocit stisknutim CTRL+C, nebo ukoncenim procesu jinym zpusobem 
    print("Startuji aplikacni smycku...\nPro ukonceni stisknete CTRL+C, nebo zabijte proces")
    try:
        tornado.ioloop.IOLoop.current().start()
    except KeyboardInterrupt:
        print("Nashledanou a přijďte zas :-)")
