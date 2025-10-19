# Práce s bitmapou radaru pomocí knohovny Pillow: https://pillow.readthedocs.io/en/stable/
from PIL import Image
from PIL import ImageDraw

# Webový serve rpomocí knihovny Tornado: https://www.tornadoweb.org/en/stable/
import tornado.ioloop
import tornado.web

# Plánování stahovaní nového snímku každých pět minut pomocí knihovny Schedule: https://schedule.readthedocs.io/en/stable/
import schedule

# Stahování snímku ze serveru ČHMÚ pomocí knihovny Requests: https://requests.readthedocs.io/en/latest/
import requests

# Další knihovny jsou už vestavěno usoučástí Pythonu; používáme nejnovější dostupnou verzi!
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo
import time
import math
from io import BytesIO
from collections import deque
import os
import glob

# Konfigurace serveru a vyhledávače deště
HTTPS = True           # Má webový server používat HTTPS? (certifikáty si musíte zajistit svépomocí)
LOGGING = False        # Máme logovat do souboru/stdout?
DRAWTESTIMAGE = False  # Máme kreslit tetsovací snímky s vyznačením nejbližšího nalezeného deště oproti pozorovateli?

# Zdroj radarových snímků
# 'network', nebo 'filesystem'. Filesystem se hodí při vývoji, kdy nechceme ze serveru ČHMÚ stahovat čerstvé snímky třeba v situaci, když neprší
# Pokud jsme nastavili 'filesystem', ale ve složce RADARIMAGESPATH není žádný PNG snímek radaru, stáhne a uloží se čerstvý 
RADARIMAGESOURCE = "network" 
RADARIMAGESPATH = os.path.join(os.path.dirname(__file__), "data")

RADARIMAGETYPE = "CAPPI" # 'CAPPI', nebo 'MAXZ' typ snímku. MAXZ je řez celým výškovým sloupcem (odhalí jádro bouřky výše v atmosféře). CAPPI je řez ve 2 km a odpovídá spíše vypadávajícímu dešti
MAXDISTANCELIMIT = 50 # Nejbližšší déšť budeme hledat jen do vzdálenosti cca 50 km (50 px). Čím větší vzdálenost, tím delší výpočet zvoleného algorimtu! Při nasazení na serveru se hodí spíše 20-30 km

# Výchozí parametry vyhledávače deště
DEFAULT_PRECIP_THRESHOLD = 0.1 # Detekujeme pouze pixely, které představují déšť s tímto úhrnem mm/hod. V praxi se hodí nastavit třeba  1 mm/h. Viz barevná stupnice radaru na webu ČHMÚ 
DEFAULT_SIZE_THRESHOLD = 6     # Detekujeme pouze déšť, který v daném okně obsahuje alespoň tolik pixelů. Ignorujeme maličké a osamocené šmouhy
DEFAULT_WINDOW = 10            # Velikost vyheldávacího okna v pixelech (1 px = cca 1km). Pokud narazíme na pixel deště, v tomto okně okolo něj začneme počítat pixely (viz výše)
DEFAULT_MAX_DISTANCE = 20      # Výchozí maximální vzdálenost deště. Viz výše. MAXDISTANCELIMIT stabovuje maximální možný limit, DEFAULT_MAX_DISTANCE ten výchozí, pokud skrze URL nenastavíme jiný

radarimage = None              # Objekt radarového snímku
radardatum_orig = None         # Originální UTC datum snímku
radardatum_local = None        # Přepočítané místní datumn (SEĆ/SELČ)
pixels = None                  # Pole pxielů radarového snímku, ve kterém budeme vyhledávat déšť

# LUT tabulka pro přepočet dBz na hodinový úhrn srážek
# Ručně opsáno podle barevné stupncie na webu ČHMÚ
DBZLUT = {
    4: 0.1,
    8: 0.325,
    12: 0.55,
    16: 0.775,
    20: 1.0,
    24: 3.25,
    28: 5.5,
    32: 7.75,
    36: 10.0,
    40: 28.0,
    44: 46.0,
    48: 64.0,
    52: 82.0,
    56: 100.0,
    60: 100.0
}

# Případné primitivní logování do souboru ./log.txt včetně časové značky
def printl(txt):
    if LOGGING:
        with open("./log.txt", "a") as f:
            dt = datetime.today().strftime('%H:%M:%S.%f %d.%m.%Y')
            f.write(f"{dt}\t{txt}\n")
            print(f"{dt}\t{txt}\n", flush=True)

# Funkce pro stažení čerstvého radarového snímku
def getRadarImage(type=None, datum=None, attempts=5, minutes_step=5, source=None):
    global radarimage, radardatum_orig, radardatum_local, pixels

    if source == None:
        source = RADARIMAGESOURCE

    if type == None:
        type = RADARIMAGETYPE

    # Pokud je zdrojem filesystem, pokusíme se načíst obrázek podle názvu ze složky RADARIMAGESPATH
    # Obrázke musí mít název ve formátu radar_YYYYMMDDHHMMSS.png (UTC zóna, třeba radar_202510061150.png)
    if source == "filesystem":
        files = glob.glob(f"{RADARIMAGESPATH}/radar_*.png")
        if files:
            radarimage = Image.open(files[0])
            radarimage = radarimage.convert("RGBA")
            pixels = radarimage.load()
            shortname = os.path.splitext(os.path.basename(files[0]))[0]
            ts = shortname.split("_", 1)[1]                      
            radardatum_orig = datetime.strptime(ts, "%Y%m%d%H%M").replace(tzinfo=timezone.utc)
            radardatum_local = radardatum_orig.astimezone(ZoneInfo("Europe/Prague"))
            return True
        # Pokud ve složce obrázek není, pokusíme se stáhnout čerstvý a uložíme ho do složky
        else:
            if getRadarImage(type, datum, attempts, minutes_step, "network"):
                radarimage.save(f"{RADARIMAGESPATH}/radar_{radardatum_orig.strftime('%Y%m%d%H%M')}.png")
                return True
            else:
                return False
    # Pokud je zdrojem network, stáhneme čerstvý snímek ze serveru ČHMÚ    
    # Pokud tam podle aktuálního času ještě není aktuální soubor, ubereme pět minut a stáhneme starší
    elif source == "network":
        if datum == None:
            datum = datetime.now(timezone.utc) 
        datum = datum.replace(minute=datum.minute - (datum.minute % minutes_step), second=0, microsecond=0)
        while attempts > 0:
            datum_txt = datum.strftime("%Y%m%d.%H%M")
            if type == "CAPPI":
                url = f"https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/data/czrad-z_cappi020/pacz2gmaps3.z_cappi020.{datum_txt}.0.png"
            elif type == "MAXZ":
                url = f"https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/data/czrad-z_max3d/pacz2gmaps3.z_max3d.{datum_txt}.0.png"
            else:
                return False           
            printl(f"Stahuji soubor: {url}")
            r = requests.get(url)
            if r.status_code != 200:
                printl(f"HTTP {r.status_code}: Nemohu stáhnout soubor")
                printl("Pokusím se stáhnout o 5 minut starší soubor")
                datum -= timedelta(minutes=minutes_step)
                attempts -= 1
                time.sleep(.5)
            else:
                radardatum_orig = datum
                radardatum_local = datum.astimezone(ZoneInfo("Europe/Prague"))
                radarimage = Image.open(BytesIO(r.content))
                radarimage = radarimage.convert("RGBA")
                pixels=radarimage.load()
                return True
        return False
    
    else:
        return False

# Výpočet vzdálenosti mezi dvěma pixelovými souřadnicemi
def getPixelDistance(x1, y1, x2, y2):
    return math.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2)

# Výpočet kilometrové vzdálenosti na kouli
# Pro naše potřeby dostatečně přesné vzhledem k měřítku radarové mapy
def getKmDistance(lat1, lon1, lat2, lon2):
    # Poloměr Země na 50. rovnoběžce. Zhruba vertikální střed republiky
    R = 6377.5
    # Převedení stupňů na radiány
    lat1_rad = math.radians(lat1)
    lon1_rad = math.radians(lon1)
    lat2_rad = math.radians(lat2)
    lon2_rad = math.radians(lon2)
    # Výpočet změny souřadnic
    dlat = lat2_rad - lat1_rad
    dlon = lon2_rad - lon1_rad
    # Haversinův vzorec
    a = math.sin(dlat / 2)**2 + math.cos(lat1_rad) * math.cos(lat2_rad) * math.sin(dlon / 2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    # Vzdálenost v kilometrech
    return R * c

# Výpočet azimutu (xo/yo = pozorovatel/observer, xt/yt = target)
# Úhel počítáme od severu. Takže když bude target oproti pozorovateli přesně na východě, získáme 90 stupňů atp.
def getAzimuth(xo, yo, xt, yt):
    dx = xt - xo
    dy = yo - yt
    az = math.degrees(math.atan2(dx, dy)) % 360.0
    return az

# Výpočet dBz deště podle barvy pixelu. Pokud barva přesně neodpovídá, použijeme tu nejbližší
# Vždycky bycho mtedy měli dostat nějaké dBz, i kdyby barva přesně neodpovídala. Pozor tedy na případné pomocné prvky ve scéně, které nepředsatvují déšť
# Jejich barvy bychom zde mohl ignorovat; třeba černé textové popisky, rám atd.
# Ručně opsáno podle barevné stupnice na snímku ČHMÚ
def colorToDbz(r,g,b):
    color_to_level = {
        (56, 0, 112): 4,
        (48, 0, 168): 8,
        (1, 1, 246): 12,
        (0, 108, 192): 16,
        (0, 160, 0): 20,
        (0, 188, 0): 24,
        (52, 216, 0): 28,
        (152, 215, 1): 32,
        (219, 215, 1): 36,
        (252, 176, 0): 40,
        (252, 132, 0): 44,
        (252, 88, 0): 48,
        (252, 0, 0): 52,
        (153, 2, 2): 56,
        (252, 252, 252): 60
    }
    best_level = 0
    best_dist = float("inf")
    for (cr, cg, cb), level in color_to_level.items():
        dist = (r - cr) ** 2 + (g - cg) ** 2 + (b - cb) ** 2
        if dist < best_dist:
            best_dist = dist
            best_level = level
    return best_level

# Funkce pro komplexní vyhledání deště pomocí algorimtu BFS / Prohledávání do šířky
# https://cs.wikipedia.org/wiki/Prohled%C3%A1v%C3%A1n%C3%AD_do_%C5%A1%C3%AD%C5%99ky
def findNearestRainBFS(x: int, y: int,window: int, size_threshold: int, precipitation_threshold: float, max_distance: int):
    width, height = radarimage.size

    # Pomocná vnořená funkce pro zjištění, že jsme ve hranicích
    def inBounds(ix: int, iy: int) -> bool:
        return 0 <= ix < width and 0 <= iy < height
    
    # Pomocná vnořená funkce pro zjištění, že má daný pixel nenulovou RGBA barvu, takže to asi bude déšť (pozor na pomocnou grafiku ve scéně! Zatím neošetřeno)
    def isRainAtPos(ix: int, iy: int):
        r, g, b, a = pixels[ix, iy]
        return (a > 0 and (r | g | b) != 0, r, g, b)

    # Pomocná funkce pro spočítání barevných pixelů v okolním okně pro zjištění, jestli déšť splňuje podmínku velikosti
    # Vrátíme i statistiku nejsilnějšího nalezeného dBz v okně a nejsilnějšího pixelu deště
    def countRainyPixelsInArea(cx: int, cy: int):
        dbz_max = -1
        r_max = 0
        g_max = 0
        b_max = 0
        pol = window // 2
        x0 = max(0, cx - pol)
        y0 = max(0, cy - pol)
        x1 = min(width - 1, cx + pol)
        y1 = min(height - 1, cy + pol)
        count = 0
        for iy in range(y0, y1 + 1):
            for ix in range(x0, x1 + 1):
                rain, r, g, b = isRainAtPos(ix, iy)
                if rain:
                    count += 1
                    dbz = colorToDbz(r, g, b)
                    if dbz > dbz_max:
                        dbz_max = dbz
                        r_max = r
                        g_max = g
                        b_max = b
        return count, dbz_max, r_max, g_max, b_max

    # Výchozí nastavení procházení grafu
    # max_d2 je maximální čtverec, ve kterém budeme hledat
    max_d2 = max_distance * max_distance
    queue = deque()
    visited = set()

    # Pokud je start uvnitř obrázku, nastavíme jej jako začátek vyhledávací fronty
    if inBounds(x, y):
        queue.append((x, y))

    # Vychozí nastavení sousedů = okolních vrcholl = pixelů
    neighbors = ((1, 0), (-1, 0), (0, 1), (0, -1))

    # A tady už procházíme graf sousedských vrcholů/pixelů od výchozí pozice a hledáme déšť
    while queue:
        cx, cy = queue.popleft()
        if (cx, cy) in visited:
            continue
        visited.add((cx, cy))

        if not inBounds(cx, cy):
            continue

        # Pokud jsme mimo čtverec, ve kterém chceme hledat, nepřidáme do vyhledávací fronty další prvky
        # a skočíme na další cyklus. Tím pádem grafový vyhledávač dokončí práci a vrátí None = žádný déšť
        if max_d2 is not None:
            dx = cx - x
            dy = cy - y
            if dx*dx + dy*dy > max_d2:
                continue

        # Jakmile při procházení narazíme na další pozici, zjistíme, jestli má tento pixel nenulovou barvu
        # Pokud tomu tak je a zároveň barva odpovídá minimálnímu úhrnu srážek v mm/h, který hledáme,
        # začneme počítat veliksot deště v okolním okně. Pokud i ta splní minimální požadavek,
        # ukončíme další průchod grafem a vrátíme všechnny popisné informace
        rain, r, g, b = isRainAtPos(cx, cy)
        dbz = colorToDbz(r, g, b)
        precipitation = DBZLUT.get(dbz, 0.0)
        if rain and precipitation >= precipitation_threshold:
            rainy_pixels, dbz_max, r_max, g_max, b_max = countRainyPixelsInArea(cx, cy)
            if rainy_pixels >= size_threshold:
                return cx, cy, r, g, b, dbz, dbz_max, precipitation, r_max, g_max, b_max

        # Pokračujeme v průchodu a přidáme do fronty další sousedy, jak se vzdalujeme od výchozí pozice
        for dx, dy in neighbors:
            nx, ny = cx + dx, cy + dy
            if not inBounds(nx, ny) or (nx, ny) in visited:
                continue
            if max_d2 is not None:
                rx = nx - x
                ry = ny - y
                if rx*rx + ry*ry > max_d2:
                    continue
            queue.append((nx, ny))

    # Na toto místo se dostaneme pouze v případě, že jsme nenašli žádný déšť
    return None, None, None, None, None, None, None, None, None, None, None


# Funkce komplexního procesoru, který ověří, že je k dispozici radarový snímek, vyhledá na něm déšť a vrátí odpověď ve formátu JSON
# Funkci je možné volat i samostatně bez spuštěného serveru
def rainProcessor(lat = None, lon = None, precipitation_threshold = None, size_threshold = None, window = None, max_distance = None, draw_test_image = None):
    
    # Pokud nenastavíme některou z konfiguračních kodnot, použijeme tu výchozí
    if precipitation_threshold == None:
        precipitation_threshold = DEFAULT_PRECIP_THRESHOLD

    if size_threshold == None:
        size_threshold = DEFAULT_SIZE_THRESHOLD

    if window == None:
        window = DEFAULT_WINDOW

    if max_distance == None:
        max_distance = DEFAULT_MAX_DISTANCE
    
    if draw_test_image == None:
        draw_test_image = DRAWTESTIMAGE

    # Základ JSON odpovědi
    response = {
        "return": True,
        "img_source": RADARIMAGESOURCE,
        "img_time": radardatum_local.strftime("%H:%M"),
        "setup": {
            "lat": lat,
            "lng": lon,
            "precipitation_threshold": precipitation_threshold,
            "size_threshold": size_threshold,
            "window": window,
            "max_distance": max_distance,
            "draw_test_image": draw_test_image,
        }
    }

     # Pokud nevyplníme lat (zeměpisná šířka) a lon (zeměpisná délka), ukončíme zpracovávání a vrátíme chybový kód v JSON  
    if lat == None or lon == None:
        response.update({
                "return": False,
                "error": "Wrong lat or lon"
        })
        return response

    # Pokud mnemáme radarový snímek, pokudíme se jej stáhnout
    # Pokud to selže, vrátíme JSON s chybovým kódem
    if radarimage == None:
        if getRadarImage() == False:
            response.update({
                "return": False,
                "error": "Can't get radar image"
            })
            return response

    # Zeměpisné souřadnice levého horního a pravého dolního okraje radarvoho snímku
    # Vzhledem k měřítku neprovádíme žádno usofistikovanou geografickou projekci,
    # ale jen přepočítámš XY souřadnice na zeměpisné a naopak
    lon0 = 11.2673442
    lat0 = 52.1670717
    lon1 = 20.7703153
    lat1 = 48.1
    degree_width = lon1 - lon0
    degree_height = lat0 - lat1
    lat_pixel_size = degree_height / radarimage.height
    lon_pixel_size = degree_width / radarimage.width

    # Pixelové souřadsnice XY pozorovatele ze souřadnice lat/lon
    x = int((lon - lon0) / lon_pixel_size)
    y = int((lat0 - lat) / lat_pixel_size)

    # Provedeme BFS vyhledání deště a změříme čas pro účely statistiky
    # Na času je krásně vidět, jak dramaticky roste, pokud neomezíme vyhledávání na maximální vzdálenost
    # Mohli bychom implementovat další optimalizace, kešování některých výpočtů, anebo použít úplně jiný algoritmus
    t = time.time()
    rainX, rainY, r, g, b, dbz, dbz_max, precipitation, r_max, g_max, b_max = findNearestRainBFS(x, y, window, size_threshold, precipitation_threshold, max_distance)
    delta = int((time.time() - t) * 1000)

    # Pokud jsme nalezli déšť
    if rainX != None and rainY != None:
        # Pokud je aktivní kreslení tetsovacího obrázku, vytvoříme ho pod názvem computed_radar_YYYYMMDDHHMM_HHMMSS.png (první datum odpovídá radarovému zdroji, druhé okamžiku kreslení)
        if draw_test_image:
            imgtmp = radarimage.copy()
            draw = ImageDraw.Draw(imgtmp, "RGBA")
            half = window // 2
            bbox_rain = [rainX - half, rainY - half, rainX + half, rainY + half]
            draw.line([(x, y), (rainX, rainY)], fill=(255, 0, 0), width=1)
            draw.rectangle(bbox_rain, outline=(255, 0, 0), fill=None, width=1)
            dt = datetime.now().strftime("%H%M%S")
            imgtmp.save(f"{RADARIMAGESPATH}/computed_radar_{radardatum_orig.strftime('%Y%m%d%H%M')}_{dt}.png")
            response["test_image"] = f"/get/computed_radar_{radardatum_orig.strftime('%Y%m%d%H%M')}_{dt}.png"

        # Naplníme JSON informací, kde se déšť nachází, jako umá veliksot/sílu atp.
        d = getPixelDistance(x, y, rainX, rainY)
        azimuth = getAzimuth(x, y, rainX, rainY)
        destLon = rainX * lon_pixel_size + lon0
        destLat = lat0 - rainY * lat_pixel_size
        dKm = getKmDistance(lat, lon, destLat, destLon)
        response.update({
            "rain": True,
            "distance_px": round(d, 2),
            "distance_km": round(dKm, 2),
            "azimuth": round(azimuth),
            "lat": round(destLat, 7),
            "lon": round(destLon, 7),
            "color": {"r": r, "g": g, "b": b},
            "color_max": {"r": r_max, "g": g_max, "b": b_max},
            "dbz_nearest": dbz,
            "dbz_max": dbz_max,
            "mmh_nearest": precipitation,
            "mmh_max": DBZLUT.get(dbz_max, 0.0),
            "computation_ms": delta
        })
    # Pokud jsme déšť nenašli, vrátíme JSN s popisem aktruální konfigurace a informací rain = false
    else:
        response["rain"] = False
        response["computation_ms"] = delta

    return response
    
# Pokud aplikace poběží jako server, tato funkce nastartuje automatické stahování snímku každých pět minut¨
# Volíme časy vždy minutu po celé pětiminutovce, tou dobou už totiž bude snímek s velkou pravděpodobností připravený na serveru ČHMÚ  
def initScheduler():
    for m in range(1, 60, 5):  # 1. 6. 11. 16. minuta atd.
        schedule.every().hour.at(f":{m:02d}").do(getRadarImage)

# Pomocná funkce pro časovač plánovače výše. Tato funkce se volá každou sekundu
def secondTick():
    schedule.run_pending()

# Text výchozí odpovědi webového serveru s nápovědou, jak použít API
def defaultHTMLAnswer():
    string = """
        Doporučené použití:
            ?task=nearest&lat=49.195090&lng=16.606162&precipitation_threshold=1&size_threshold=6&window=10&max_distance=20&draw_test_image=false

        Parametry URL:
            - lat: Zemepisna sirka
            - lng: Zemepisna delka
            - precipitation_threshold: Dest musi dosahovat alespon tohoto uhrnu srazek v mm/h
            - size_threshold: V okne deste musi byt alespon tolik pixelu deste
            - window: Pixelova sirka okolniho okna deste, ktere zacneme prohledavat, pokud narazime na nejblizsi dest
            - max_distance: Maximalni pixelova vzdalenost, ve ktere hledame dest
            - draw_test_image: Vygeneruje testovaci snimek s vyznacenim nejblizsiho deste podle konfigurace vyse
        
        Cim vyssi hodnota max_distance, tim narocnejsi vyhledavani algoritmem BFS a dramaticky delsi doba vypoctu, protoze aplikace bezi na velm islabem serveru. Udrzujte idealne do 15-30 px, coz odpovida zhruba 15-30 km. To by melo bohate stacit - cilem neni hledat dest 100 kilometru daleko. Maximalni hodnota je 50!
    """
    return string
    
# Třída HTTP požadavku /, ze kterého zavoláme rainPRocessor a vrátíme HTTP klinetovi odpověď    
class HttpIndex(tornado.web.RequestHandler):
    def initialize(self, **kwargs):
        super().initialize(**kwargs)
    def get(self):
        task = self.get_argument("task", default="")
        if task == "nearest":

            lat = self.get_argument("lat", default=None)
            lng = self.get_argument("lng", default=None)

            if lat is None or lng is None:
                self.set_header("Content-Type", "text/plain; charset=UTF-8")
                self.write(defaultHTMLAnswer())
                self.finish()
                return
            else:
                lat = float(lat)
                lng = float(lng)

            precThreshold = self.get_argument("precipitation_threshold", default=None)
            precThreshold = float(precThreshold) if precThreshold is not None else DEFAULT_PRECIP_THRESHOLD

            sizeThreshold = self.get_argument("size_threshold", default=None)
            sizeThreshold = int(sizeThreshold) if sizeThreshold is not None else DEFAULT_SIZE_THRESHOLD

            window = self.get_argument("window", default=None)
            window = int(window) if window is not None else DEFAULT_WINDOW

            maxDistance = self.get_argument("max_distance", default=None)
            maxDistance = min(int(maxDistance), MAXDISTANCELIMIT) if maxDistance is not None else DEFAULT_MAX_DISTANCE

            drawTestImage = self.get_argument("draw_test_image", default=None)
            drawTestImage = True if drawTestImage and drawTestImage.lower() in ("1", "true", "yes", "on") else DRAWTESTIMAGE

            data = rainProcessor(lat, lng, precThreshold, sizeThreshold, window, maxDistance, drawTestImage)
         
            self.set_header("Content-Type", "application/json")
            self.write(data)
            self.finish()
        else:
            self.set_header("Content-Type", "text/plain; charset=UTF-8")
            self.write(defaultHTMLAnswer())
            self.finish()

# Začátek programu, pokud skript spouštíme přímo a nepoužíváme jej jako knihovnu   
if __name__ == "__main__":
    # Stáhneme čerstvý snímek (nebo h odle konfigurace načteme ze souboru)
    getRadarImage()
    # Nastartujeme časovač pro automatické stahování (v případě nastavení na zdroj filesystem se prostě znovu načte ze souboru) 
    initScheduler()

    
    # Nastartování serveru, který reaguje na HTTP GET dotazy:
    #  / = naše API
    # /get/* = vrátí soubor * z adresáře RADARIMAGESPATH
    web = tornado.web.Application([
        (r'/?', HttpIndex),
        (r"/get/(.*)", tornado.web.StaticFileHandler, {
            "path": RADARIMAGESPATH,
            "default_filename": None
        })
    ])
    # Web poslouchá na standardním TCP portu 80
    # Pozor, na Linuxu potřebujete patřičná práva, nebo změňte port na jiný
    web.listen(80)

    # Pokud jsme povolili HTTPS, načteme certifikáty
    # Toto je ukázka pro bezplatné certifikáty Let's Encrypt, doménu radar.kloboukuv.cloud a výchozí cesty k certifikátpm LE na Linuxu 
    if HTTPS:
        https = tornado.httpserver.HTTPServer(
            web,
            ssl_options = {
                "certfile": "/etc/letsencrypt/live/radar.kloboukuv.cloud/fullchain.pem",
                "keyfile": "/etc/letsencrypt/live/radar.kloboukuv.cloud/privkey.pem"
            }
        )
        # HTTPS poslouchá na standaredním TCP portu 443
        https.listen(443)

    # Nastartujeme pomocný časovač secondTick pro řízení plánovače, který se bude volat systémovou smyčkou každých 1000 ms
    secondTickTask = tornado.ioloop.PeriodicCallback(secondTick, 1000)
    secondTickTask.start()

    # NAstartujeme systémovou smyčku, která drží server při životě
    printl("Startuji aplikační smyčku...")
    tornado.ioloop.IOLoop.current().start()
