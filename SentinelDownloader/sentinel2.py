# Pro OAuth se pouziva https://oauthlib.readthedocs.io/en/latest/installation.html
# Nainstaluj smudlo!
from oauthlib.oauth2 import BackendApplicationClient
from requests_oauthlib import OAuth2Session
from datetime import datetime, timedelta, timezone
import math, argparse

# Nastaveni API OAuth (musis se zaregistrovat)
# https://shapps.dataspace.copernicus.eu/dashboard/#/account/settings
client_id = ""
client_secret = ""

if len(client_id) == 0 or len(client_secret) == 0:
    print("Vypln client_id a client_secret pro OAuth autentizaci!")
    print("Zaloz si bezplatny ucet na Copernicus a bez sem: https://shapps.dataspace.copernicus.eu/dashboard/#/account/settings")
    exit()

# Vychozi hodnoty
VYSKA = 512 # Vychozi vyska obrazku
SIRKA_MAX = 2500 # Maximalni povolena sirka obrazku na strane API
FORMAT = "image/jpeg" # Format vystupnich dat
SOUBOR_NAZEV = "snimek.jpg" # Vystupni soubor
KVALITA = 90 # JPEG kvalita
BOX = "13.960876,49.841525,14.988098,50.296358" # GPS ouradnice ctverce Prahy

# Evalscript je specialni procesni Javascript, ktery se spousti nad daty na serverech Copernicus
# Chceme druzicova data v pravych barvach (kanaly B02, B03, B04)
# Zesilime expozici, surova data jsou totiz hodne tmava
EVALSCRIPT = """
//VERSION=3
function setup() {
  return {
    input: ["B02", "B03", "B04"],
    output: { bands: 3 },
  }
}
function evaluatePixel(sample) {
  let expozice = 2;
  return [sample.B04 * expozice, sample.B03 * expozice, sample.B02 * expozice];
}
"""

parser = argparse.ArgumentParser()
parser.add_argument("-S", "--soubor", default=SOUBOR_NAZEV, type=str, help = "Nazev souboru (praha.jpg apod.)")
parser.add_argument("-V", "--vyska", default=VYSKA, type=int, help = "Vyska obrazku v pixelech (500, 1000 apod.)")
parser.add_argument("-B", "--box", default=BOX, type=str, help = "Vyberovy obedlnik v souradnicich WGS84 (lng0,lat0,lng1,lat1)")
parser.add_argument("-F", "--format", default=FORMAT, type=str, help = "Format obrazku (image/jpeg, image/png)")
parser.add_argument("-K", "--kvalita", default=KVALITA, type=int, help = "Kvalita obrazku (0-100)")
parser.add_argument("-E", "--evalscript", default="", type=str, help = "Soubor vlastniho evalskriptu (evalscript.js)")
parser.add_argument("-J", "--jas", default=2, type=float, help = "Jas obrazku, pokud se pouzije vestaveny evalskript (0-XXX)")
parser.add_argument("-T", "--ukaztoken", action="store_true", help = "Pouze vygeneruje docasny token/API klic (lze pouzit pro Bearer autentizaci bez OAuth)")
argumenty = parser.parse_args()

# Uprava parametru podle argumentu skriptu
VYSKA = argumenty.vyska
FORMAT = argumenty.format
SOUBOR_NAZEV = argumenty.soubor
KVALITA = argumenty.kvalita
BOX = [float(souradnice) for souradnice in argumenty.box.split(",")]
EVALSCRIPT = EVALSCRIPT.replace("let expozice = 2;", f"let expozice = {argumenty.jas};")

# Pokud chceme pouzit vlastni evalskript ze souboru, prepiseme ten vychozi
if len(argumenty.evalscript) > 1:
    try:
        with open(argumenty.evalscript, "r") as f:
            EVALSCRIPT = f.read()
    except:
        print(f"Nemohu přečíst evalscript {argumenty.evalscript}. Použiji výchozí")


# Vytvorime OAuth sezeni
client = BackendApplicationClient(client_id=client_id)
oauth = OAuth2Session(client=client)
token = oauth.fetch_token(token_url='https://identity.dataspace.copernicus.eu/auth/realms/CDSE/protocol/openid-connect/token', client_secret=client_secret, include_client_id=True)

# Pokud chceme jen vypsat token/API key pro Bearer autentizaci
# Vypiseme autentizacni hodnoty a ukoncime program
if argumenty.ukaztoken:
    for key in token:
        print(f"{key}: {token[key]}")
    exit()

# Vypocet pomeru stran pro obrazek v pixelech ze zemepisnych souradnic
# Potencialne nepresne, ale jde nam o obrazek pro bezne pouziti, ne geodezii a GIS
rozdil_sirky = BOX[3] - BOX[1] 
rozdil_delky = BOX[2] - BOX[0] 
prumerna_sirka = (BOX[1] + BOX[3]) / 2 
vzdalenost_sirky = rozdil_sirky * 111  
vzdalenost_delky = rozdil_delky * 111 * math.cos(math.radians(prumerna_sirka)) 
pomer_stran = vzdalenost_delky / vzdalenost_sirky

# Vypocet sirky vystupniho obrazku podle nastavene vysky a poměru stran
sirka = int(VYSKA * pomer_stran)
vyska = VYSKA

# API umi vytvorit obrazek o sirce nejvyse 2500 pixelu
# Takze pokud jsme vypocitali vetsi sirku, zmensime vysku
if sirka > SIRKA_MAX:
    sirka = SIRKA_MAX
    vyska = int(sirka / pomer_stran)
print(f"Rozmery hotoveho obrazku: {sirka}x{vyska} pixelu")

# Druzice nad mistem/boxem v Evrope sice leti zpravidla kazde 2-3 dny,
# ale prodleva muze byt i delsi, a tak stanovime vyhledavaci rozsah na 7 dnu
datum_konec = datetime.now(timezone.utc)
datum_zacatek = (datum_konec - timedelta(days=7)).isoformat()
datum_konec = datum_konec.isoformat() 

# Priprava API dotazu ve forme JSON
# Budeme chit snimek z druzice Sentinel-2 L2A
# Pro dany box, pixelovou velikost, format atp.
# Pro dalsi mozne parametry kouknete na https://shapps.dataspace.copernicus.eu/requests-builder/ 
parametry_dotazu = {
    "input": {
        "bounds": {
            "bbox": BOX
        },
        "data": [
            {
                "type": "sentinel-2-l2a",
                "dataFilter": {
                    "timeRange": {
                        "from": datum_zacatek,
                        "to": datum_konec,
                    }
                },
                "processing": {
                    "upsampling": "BILINEAR",
                    "downsampling": "BILINEAR"
                },
            }
        ]
    },
    "output": {
        "width": sirka,
        "height": vyska,
        "responses": [
            {
                "identifier": "default",
                "format": {
                    "type": FORMAT,
                    "quality": KVALITA,
                }
            }
        ]
    },
    "evalscript": EVALSCRIPT
}

# Provedeme autentizovany HTTP dotaz skrze OAuth a server by mel odpovedet rovnou bajty obrazku
# V tomto API dotazu se (asi) nedozvime, z jakeho presneho casu ve vyberu obrazek pochazi
odpoved = oauth.post("https://sh.dataspace.copernicus.eu/api/v1/process", json = parametry_dotazu)
if odpoved.status_code == 200:
    print(f"Ukladam obrazek {SOUBOR_NAZEV}")
    with open(SOUBOR_NAZEV, "wb") as file:
        file.write(odpoved.content)
else:
    print(f"Chybicka se vloudila!\nHTTP kod: {odpoved.status_code}\n{odpoved.text}")
