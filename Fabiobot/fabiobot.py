# Ke spusteni HTTP serveru potrebujeme knihovnu Tornado
# https://www.tornadoweb.org/en/stable/
import tornado.ioloop
import tornado.web
# Pro HTTP GET/POST klient potrebujeme knihovnu Requests
# https://requests.readthedocs.io/en/latest/
import requests
import urllib.parse

# Konfigurace API aplikace na Strave
# Client ID a Client Secret najdete po vytvoreni aplikace na:
# https://www.strava.com/settings/api
strava_aplikace = {
    "client_id": XXXXX,
    "client_secret": "XXXXX" 
}

# Tato trida bude vyrizovat staticke HTTP pozadavky na cestu /fabiobot/assets
# Obrazky a dalsi data v adresari ./assets
class StaticFileHandler(tornado.web.StaticFileHandler):
    def set_extra_headers(self, path):
        self.set_header("Cache-Control", "max-age=864000")

# Tato truda bude obsluhovat OAuth autorizaci
# A take spojeni se strava API pri pozadavku /fabiobot/stats z javascipru v prohlizeci
class Fabiobot(tornado.web.RequestHandler):
    def get(self):
        r = self.request
        # Pokud je v URL "exchange_token", snazi se s nami spojit Strava a dokoncit autentizaci OAuth
        if "exchange_token" in r.path:
            kod = self.get_argument("code", default="")
            url = f"https://www.strava.com/oauth/token?client_id={strava_aplikace['client_id']}&client_secret={strava_aplikace['client_secret']}&code={kod}&grant_type=authorization_code"
            r = requests.request("POST", url)
            data = r.json()

            # Klicove pristupove informace
            # Mohli bychom je ulzoit na serveru, 
            # ale my je pro jendoduchost ulozime v prohlizeci jako HTTP Cookies
            expires_at = data["expires_at"] # Cas vyprseni pristupvoeho tokenu
            refresh_token = data["refresh_token"] # Specialn itoken pro obnovu vyprsenemho tokenu (viz https://developers.strava.com/docs/authentication/#refreshingexpiredaccesstokens)
            access_token = data["access_token"] # Pristupovy token, tedy klic, ktery budeme pouzivat pri pristupu k API a datum konkretniho uzivatele, ktery nam dal prave prava
            uzivatel_id = data["athlete"]["id"] # ID uzivatele, ktery nam prave dal prava pro pritup na jeho Stravu
            uzivatel_firstname = data["athlete"]["firstname"] # Jmeno uzivatele
            uzivatel_lastname = data["athlete"]["lastname"] # Prijmeni uzivatele

            # Pristupovy token/klic, ID uzivatele a jeho jmeno ulozime do HTTP Cookies
            self.set_signed_cookie("fabiobot_access_token", access_token, expires = expires_at)
            self.set_cookie("fabiobot_user_id", str(uzivatel_id), expires = expires_at)
            self.set_cookie("fabiobot_user_name", urllib.parse.quote(uzivatel_firstname + " " + uzivatel_lastname), expires = expires_at)

            # Autorizace uzivatele je dokoncena, takze se presmerujeme zpet na uvodni stranku
            self.redirect("/fabiobot")

        # Pokud je v URL "/fabiobot/stats", klient/JS aplikace v prohlizeci se dotazuje na statistiku uzivatele  
        elif "/fabiobot/stats" in r.path:
            # Pokud chceme statistiku uzivatele, ale nemame v HTTP Cookies pristupovy klic,
            # je to chyba, takze se presmerujeme zpet na uvodni stranku
            if not self.get_signed_cookie("fabiobot_access_token"):
                self.redirect("/fabiobot")
            # Pokud mame pristupova a popisna data o uzivateli,
            # zeptame se na celkovou statistiku Strava API>
            # https://developers.strava.com/docs/reference/#api-Athletes-getStats
            else:
                access_token = self.get_signed_cookie("fabiobot_access_token").decode()
                athlete_id = self.get_cookie("fabiobot_user_id")
                # prisrtupovy token vlozime do HTTP hlavicky ve formatu Authorization: Bearer token
                headers = {"Authorization": f"Bearer {access_token}"}
                url = f"https://www.strava.com/api/v3/athletes/{athlete_id}/stats"
                r = requests.get(url, headers = headers)
                # Odpoved ze Strava API dorazi ve formatu JSON,
                # ktery posleme zpet klinetovi/aplikaci v JS nactene v prohlizeci 
                self.set_header("Content-Type", "application/json")
                self.write(r.json())

        # V ostatnich pripadech posleme klientovi/webovemu prohlizeci HTML stranku fabiobot.html
        # Ve starnce je jak formular pro prihlaseni, tak JS aplikace pro prevod km na fabie 
        else:
            self.render("fabiobot.html")

# Bod spusteni programu v Pythonu
if __name__ == "__main__":
    # Nastartovani HTTP serveru
    # Pri pozadavku /fabiobot/* zpracujeme dotaz ve tride Fabiobot
    # Pri pozadavku /fabiobot/assets/* zpracujeme dotaz ve tride StaticFileHandler, 
    # ktera vrati odpovidajici soubor z povoleneho adresare ./assets
    # Vybare HTTP Cookies chceme sifrovat heslem 'Chceme_zpatky_kavarnu_Bonbon'
    web = tornado.web.Application([
        (r'/fabiobot/?.*', Fabiobot),
        (r"/assets/(.*)", StaticFileHandler, {"path":"assets/"}),
    ], cookie_secret="Chceme_zpatky_kavarnu_Bonbon")

    # Poslocuhame na TCP portu 80
    web.listen(80)

    # Startujeme nekonecnou smycku programu
    # Ukoncime treba stiskem CTRL+C, nebo zabitim celeho procesu jinym zpusobem
    print("Startuji aplikacni smycku...")
    tornado.ioloop.IOLoop.current().start()
