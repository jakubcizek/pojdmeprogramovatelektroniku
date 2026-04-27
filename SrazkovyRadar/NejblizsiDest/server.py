# Práce s bitmapou radaru pomocí knihovny Pillow: https://pillow.readthedocs.io/en/stable/
try:
    from PIL import Image
    from PIL import ImageDraw
except ModuleNotFoundError:
    print("Nejprve musíš nainstalovat knihovnu Pillow:\nhttps://pillow.readthedocs.io/en/stable/")
    exit()

# Webový server pomocí knihovny Tornado: https://www.tornadoweb.org/en/stable/
try:
    import tornado.web
    import tornado.ioloop
except ModuleNotFoundError:
    print("Nejprve musíš nainstalovat knihovnu Tornado:\nhttps://www.tornadoweb.org/en/stable/")
    exit()

# Plánování stahování nového snímku každých pět minut pomocí knihovny Schedule: https://schedule.readthedocs.io/en/stable/
try:
    import schedule
except ModuleNotFoundError:
    print("Nejprve musíš nainstalovat knihovnu Schedule:\nhttps://schedule.readthedocs.io/en/stable/")
    exit()

# Stahování snímku ze serveru ČHMÚ pomocí knihovny Requests: https://requests.readthedocs.io/en/latest/
try:
    import requests
except ModuleNotFoundError:
    print("Nejprve musíš nainstalovat knihovnu Requests:\nhttps://requests.readthedocs.io/en/latest/")
    exit()

# NumPy pro vektorizované varianty vyhledávače deště (radial / vectorized)
try:
    import numpy as np
except ModuleNotFoundError:
    print("Nejprve musíš nainstalovat knihovnu NumPy:\nhttps://numpy.org/install/")
    exit()

# Na některých systémech (Windows) mohou chybět informace o časových zónach, které používáme při převodu mezi UTC a Europe/Prague
# Proto si vynutíme instalaci balíčku tzdata: https://tzdata.readthedocs.io/en/latest/
try:
    import tzdata
except ModuleNotFoundError:
    print("Nejprve musíš nainstalovat knihovnu Tzdata:\nhttps://tzdata.readthedocs.io/en/latest/")
    exit()

# Další knihovny jsou už vestavěnou součástí Pythonu; používáme nejnovější dostupnou verzi!
from datetime import datetime, timedelta, timezone
from zoneinfo import ZoneInfo
import time
import math
from io import BytesIO
import os
import glob

# Konfigurace serveru a vyhledávače deště
HTTPS = True           # Má webový server používat HTTPS? (certifikáty si musíte zajistit svépomocí)
LOGGING = False        # Máme logovat do souboru/stdout?
DRAWTESTIMAGE = False  # Máme kreslit testovací snímky s vyznačením nejbližšího nalezeného deště oproti pozorovateli?

# Zdroj radarových snímků
# 'network' nebo 'filesystem'. Filesystem se hodí při vývoji, kdy nechceme ze serveru ČHMÚ stahovat čerstvé snímky třeba v situaci, když neprší.
# Pokud jsme nastavili 'filesystem', ale ve složce RADARIMAGESPATH není žádný PNG snímek radaru, stáhne se a uloží se čerstvý.
RADARIMAGESOURCE = "network" 
RADARIMAGESPATH = os.path.join(os.path.dirname(__file__), "data")

RADARIMAGETYPE = "CAPPI" # 'CAPPI' nebo 'MAXZ' – typ snímku. MAXZ je řez celým výškovým sloupcem (odhalí jádro bouřky výše v atmosféře). CAPPI je řez ve 2 km a odpovídá spíše vypadávajícímu dešti.

# Výchozí parametry vyhledávače deště
DEFAULT_PRECIP_THRESHOLD = 0.1 # Detekujeme pouze pixely, které představují déšť s tímto úhrnem mm/hod. V praxi se hodí nastavit třeba 1 mm/h. Viz barevná stupnice radaru na webu ČHMÚ.
DEFAULT_SIZE_THRESHOLD = 6     # Detekujeme pouze déšť, který v daném okně obsahuje alespoň tolik pixelů. Ignorujeme maličké a osamocené šmouhy.
DEFAULT_WINDOW = 10            # Velikost vyhledávacího okna v pixelech (1 px = cca 1 km). Pokud narazíme na pixel deště, v tomto okně okolo něj začneme počítat pixely (viz výše).
DEFAULT_MAX_DISTANCE = 20      # Výchozí maximální vzdálenost deště v pixelech (1 px ≈ 1 km), pokud klient přes URL nenastaví jinou. Vyhledávač automaticky ořeže slice na velikost snímku, takže žádný pevný horní strop už nepotřebujeme.

radarimage = None              # Objekt radarového snímku
radardatum_orig = None         # Originální UTC datum snímku
radardatum_local = None        # Přepočítané místní datum (SEČ/SELČ)

# Předpočítané NumPy matice z aktuálního radarového snímku.
# Obnovují se vždy v precomputeRadarMatrices() po stažení nového snímku, takže náklady neseme jen jednou za ~5 minut.
radar_arr = None               # H x W x 4 uint8 — surový obraz pro odečet barev
rain_mask = None               # H x W bool — pixel obsahuje déšť (alpha>0 a alespoň jeden RGB kanál != 0)
dbz_arr = None                 # H x W int8 — dBZ úroveň podle nejbližší referenční barvy (jen pro rain_mask, jinak 0)
mmh_arr = None                 # H x W float64 — odpovídající mm/h podle DBZLUT (jinak 0)
ii_arr = None                  # (H+1) x (W+1) int32 — integrální obraz rain_mask pro O(1) součet pixelů v libovolném okně

# LUT tabulka pro přepočet dBZ na hodinový úhrn srážek
# Ručně opsáno podle barevné stupnice na webu ČHMÚ
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

# Referenční barvy radaru -> dBZ úroveň. Pro každý deštivý pixel dohledáme vektorizovaně nejbližší barvu z této tabulky.
COLOR_TO_LEVEL = {
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
    (252, 252, 252): 60,
}
COLORS_ARR = np.array(list(COLOR_TO_LEVEL.keys()), dtype=np.int32)   # 15 x 3
LEVELS_ARR = np.array(list(COLOR_TO_LEVEL.values()), dtype=np.int8)  # 15

# LUT dBZ -> mm/h jako pole pro vektorové vyhledání (DBZLUT.get bez slovníku).
# Index je hodnota dBZ (0..127). Hodnoty mimo DBZLUT zůstávají 0.0.
DBZ_TO_MMH_ARR = np.zeros(128, dtype=np.float64)
for _k, _v in DBZLUT.items():
    DBZ_TO_MMH_ARR[_k] = _v

# Konstanta pro "nic nenalezeno" — vrací findNearestRain, když není v dosahu žádný kvalifikovaný déšť.
EMPTY_RESULT = (None,) * 11

# Případné primitivní logování do souboru ./log.txt včetně časové značky
def printl(txt):
    if LOGGING:
        with open("./log.txt", "a") as f:
            dt = datetime.today().strftime('%H:%M:%S.%f %d.%m.%Y')
            f.write(f"{dt}\t{txt}\n")
            print(f"{dt}\t{txt}\n", flush=True)

# Předvýpočet NumPy matic z aktuálního radarimage.
# Volá se automaticky po každém úspěšném getRadarImage() — tedy ~jednou za 5 minut, ne na každý HTTP dotaz.
# Vyrábí: radar_arr, rain_mask, dbz_arr, mmh_arr, ii_arr (popsáno u deklarace globálů).
def precomputeRadarMatrices():
    global radar_arr, rain_mask, dbz_arr, mmh_arr, ii_arr

    if radarimage is None:
        radar_arr = None
        rain_mask = None
        dbz_arr = None
        mmh_arr = None
        ii_arr = None
        return

    radar_arr = np.asarray(radarimage)  # H x W x 4
    r = radar_arr[..., 0]
    g = radar_arr[..., 1]
    b = radar_arr[..., 2]
    a = radar_arr[..., 3]

    # Pixel je déšť, pokud má neprůhlednost a alespoň jeden barevný kanál není nula (kopíruje původní isRainAtPos).
    rain_mask = (a > 0) & ((r | g | b) != 0)

    # Pro každý pixel deště najdeme nejbližší referenční barvu -> dBZ úroveň.
    # Vektorizováno přes broadcasting: rain_pixels (N x 1 x 3) - COLORS_ARR (1 x 15 x 3) -> N x 15 x 3.
    H, W = rain_mask.shape
    dbz_arr = np.zeros((H, W), dtype=np.int8)
    if rain_mask.any():
        rain_pixels = radar_arr[rain_mask, :3].astype(np.int32)            # N x 3
        diffs = rain_pixels[:, None, :] - COLORS_ARR[None, :, :]           # N x 15 x 3
        d2 = (diffs * diffs).sum(axis=2)                                   # N x 15
        nearest = d2.argmin(axis=1)                                        # N
        dbz_arr[rain_mask] = LEVELS_ARR[nearest]

    mmh_arr = DBZ_TO_MMH_ARR[dbz_arr.astype(np.int32)]  # H x W float64 — odpovídá DBZLUT.get(dbz, 0.0) per pixel

    # Integrální obraz rain_mask -> O(1) počet deštivých pixelů v libovolném obdélníku.
    # ii_arr má rozměr (H+1, W+1) a ii_arr[i, j] = součet rain_mask[0:i, 0:j].
    ii_arr = np.zeros((H + 1, W + 1), dtype=np.int32)
    ii_arr[1:, 1:] = rain_mask.astype(np.int32).cumsum(0).cumsum(1)


# Funkce pro stažení čerstvého radarového snímku
def getRadarImage(type=None, datum=None, attempts=5, minutes_step=5, source=None):
    global radarimage, radardatum_orig, radardatum_local

    if source == None:
        source = RADARIMAGESOURCE

    if type == None:
        type = RADARIMAGETYPE

    # Pokud je zdrojem filesystem, pokusíme se načíst obrázek podle názvu ze složky RADARIMAGESPATH.
    # Obrázek musí mít název ve formátu radar_YYYYMMDDHHMM.png (UTC zóna, třeba radar_202510061150.png).
    if source == "filesystem":
        files = glob.glob(f"{RADARIMAGESPATH}/radar_*.png")
        if files:
            radarimage = Image.open(files[0])
            radarimage = radarimage.convert("RGBA")
            shortname = os.path.splitext(os.path.basename(files[0]))[0]
            ts = shortname.split("_", 1)[1]                      
            radardatum_orig = datetime.strptime(ts, "%Y%m%d%H%M").replace(tzinfo=timezone.utc)
            radardatum_local = radardatum_orig.astimezone(ZoneInfo("Europe/Prague"))
            precomputeRadarMatrices()
            return True
        # Pokud ve složce obrázek není, pokusíme se stáhnout čerstvý a uložíme ho do složky.
        else:
            if getRadarImage(type, datum, attempts, minutes_step, "network"):
                radarimage.save(f"{RADARIMAGESPATH}/radar_{radardatum_orig.strftime('%Y%m%d%H%M')}.png")
                return True
            else:
                return False
    # Pokud je zdrojem network, stáhneme čerstvý snímek ze serveru ČHMÚ.    
    # Pokud tam podle aktuálního času ještě není aktuální soubor, ubereme pět minut a stáhneme starší.
    elif source == "network":
        if datum == None:
            datum = datetime.now(timezone.utc) 
        datum = datum.replace(minute=datum.minute - (datum.minute % minutes_step), second=0, microsecond=0)
        while attempts > 0:
            datum_txt = datum.strftime("%Y%m%d.%H%M")
            if type == "CAPPI":
                url = f"https://intranet.chmi.cz/files/portal/docs/meteo/rad/inca-cz/data/czrad-z_cappi020/pacz2gmaps3.z_cappi020.{datum_txt}.0.png"
            elif type == "MAXZ":
                url = f"https://intranet.chmi.cz/files/portal/docs/meteo/rad/inca-cz/data/czrad-z_max3d/pacz2gmaps3.z_max3d.{datum_txt}.0.png"
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
                precomputeRadarMatrices()
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

# Pomocná funkce: spočítá v okně okolo (cx, cy) maximální dBZ a barvu nejsilnějšího pixelu.
def _windowMaxStats(cx, cy, half, H, W):
    bx0 = max(0, cx - half)
    by0 = max(0, cy - half)
    bx1 = min(W - 1, cx + half)
    by1 = min(H - 1, cy + half)
    win_dbz = dbz_arr[by0:by1 + 1, bx0:bx1 + 1]
    win_mask = rain_mask[by0:by1 + 1, bx0:bx1 + 1]
    masked = np.where(win_mask, win_dbz, np.int8(-1))
    flat = masked.argmax()
    py, px = np.unravel_index(flat, masked.shape)
    dbz_max = int(masked[py, px])
    if dbz_max < 0:
        return -1, 0, 0, 0
    abs_y = by0 + int(py)
    abs_x = bx0 + int(px)
    r_max = int(radar_arr[abs_y, abs_x, 0])
    g_max = int(radar_arr[abs_y, abs_x, 1])
    b_max = int(radar_arr[abs_y, abs_x, 2])
    return dbz_max, r_max, g_max, b_max


# Plně vektorové hledání nejbližšího deště.
# Pro každý kandidátní pixel ve výseku [y±max_dist, x±max_dist] spočítáme:
#   * vzdálenost² od pozorovatele (broadcasting)
#   * mm/h z předpočítaného mmh_arr
#   * počet deštivých pixelů v okně přes vektorové dotazy do integrálního obrazu
# Jednou maskou pak odřežeme nevhodné kandidáty a argmin vybere euklidovsky nejbližšího. Bez Python smyčky po pixelech.
# Funguje korektně i pro pozorovatele těsně mimo bitmapu — jen středu hledání, ne startovní pozice.
def findNearestRain(x, y, window, size_threshold, precipitation_threshold, max_distance):
    if rain_mask is None:
        return EMPTY_RESULT
    H, W = rain_mask.shape

    # Výsek obrazu, který může obsahovat kandidáty (ostatní pixely jsou mimo poloměr).
    x0g = max(0, x - max_distance)
    y0g = max(0, y - max_distance)
    x1g = min(W, x + max_distance + 1)
    y1g = min(H, y + max_distance + 1)
    if x1g <= x0g or y1g <= y0g:
        return EMPTY_RESULT

    sub_mmh = mmh_arr[y0g:y1g, x0g:x1g]

    # Vzdálenost² každého pixelu výseku od pozorovatele (broadcasting v ogrid).
    yy, xx = np.ogrid[y0g:y1g, x0g:x1g]
    d2 = (xx - x) ** 2 + (yy - y) ** 2

    # Počet deštivých pixelů ve window×window okně kolem každého pixelu výseku — vektorová indexace ii_arr.
    half = window // 2
    ys = np.arange(y0g, y1g)
    xs = np.arange(x0g, x1g)
    yt = np.clip(ys - half,     0, H)        # y0
    yb = np.clip(ys + half + 1, 0, H)        # y1+1 (exclusive v ii)
    xl = np.clip(xs - half,     0, W)        # x0
    xr = np.clip(xs + half + 1, 0, W)        # x1+1
    counts = (ii_arr[yb[:, None], xr[None, :]]
              - ii_arr[yt[:, None], xr[None, :]]
              - ii_arr[yb[:, None], xl[None, :]]
              + ii_arr[yt[:, None], xl[None, :]])

    valid = (sub_mmh >= precipitation_threshold) & (counts >= size_threshold) & (d2 <= max_distance * max_distance)
    if not valid.any():
        return EMPTY_RESULT

    # Nejbližší kandidát — argmin nad maskovaným d2.
    d2_masked = np.where(valid, d2, np.iinfo(np.int64).max)
    flat_idx = int(d2_masked.argmin())
    ly, lx = np.unravel_index(flat_idx, d2_masked.shape)
    cy = int(ly) + y0g
    cx = int(lx) + x0g

    dbz_max, r_max, g_max, b_max = _windowMaxStats(cx, cy, half, H, W)
    r = int(radar_arr[cy, cx, 0])
    g = int(radar_arr[cy, cx, 1])
    b = int(radar_arr[cy, cx, 2])
    d = int(dbz_arr[cy, cx])
    precipitation = float(mmh_arr[cy, cx])
    return cx, cy, r, g, b, d, dbz_max, precipitation, r_max, g_max, b_max


# Funkce komplexního procesoru, který ověří, že je k dispozici radarový snímek, vyhledá na něm déšť a vrátí odpověď ve formátu JSON.
# Funkci je možné volat i samostatně bez spuštěného serveru.
def rainProcessor(lat = None, lon = None, precipitation_threshold = None, size_threshold = None, window = None, max_distance = None, draw_test_image = None):
    
    # Pokud nenastavíme některou z konfiguračních hodnot, použijeme tu výchozí
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
        "radar_type": RADARIMAGETYPE,
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

     # Pokud nevyplníme lat (zeměpisná šířka) a lon (zeměpisná délka), ukončíme zpracování a vrátíme chybový kód v JSON.  
    if lat == None or lon == None:
        response.update({
                "return": False,
                "error": "Wrong lat or lon"
        })
        return response

    # Pokud nemáme radarový snímek, pokusíme se jej stáhnout.
    # Pokud to selže, vrátíme JSON s chybovým kódem.
    if radarimage == None:
        if getRadarImage() == False:
            response.update({
                "return": False,
                "error": "Can't get radar image"
            })
            return response

    # Zeměpisné souřadnice levého horního a pravého dolního okraje radarového snímku.
    # Vzhledem k měřítku neprovádíme žádnou sofistikovanou geografickou projekci,
    # ale jen přepočítáme XY souřadnice na zeměpisné a naopak.
    lon0 = 11.2673442
    lat0 = 52.1670717
    lon1 = 20.7703153
    lat1 = 48.1
    degree_width = lon1 - lon0
    degree_height = lat0 - lat1
    lat_pixel_size = degree_height / radarimage.height
    lon_pixel_size = degree_width / radarimage.width

    # Pixelové souřadnice XY pozorovatele ze souřadnic lat/lon
    x = int((lon - lon0) / lon_pixel_size)
    y = int((lat0 - lat) / lat_pixel_size)

    # Vyhledáme nejbližší déšť a změříme čas pro účely statistiky.
    t = time.time()
    rainX, rainY, r, g, b, dbz, dbz_max, precipitation, r_max, g_max, b_max = findNearestRain(
        x, y, window, size_threshold, precipitation_threshold, max_distance
    )
    delta = int((time.time() - t) * 1000)

    if rainX is not None and rainY is not None:
        # Pokud je aktivní kreslení testovacího obrázku, vytvoříme ho pod názvem computed_radar_YYYYMMDDHHMM_HHMMSS.png
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

        d_px = getPixelDistance(x, y, rainX, rainY)
        azimuth = getAzimuth(x, y, rainX, rainY)
        destLon = rainX * lon_pixel_size + lon0
        destLat = lat0 - rainY * lat_pixel_size
        dKm = getKmDistance(lat, lon, destLat, destLon)
        response.update({
            "rain": True,
            "distance_px": round(d_px, 2),
            "distance_km": round(dKm, 2),
            "azimuth": round(azimuth),
            "lat": round(destLat, 7),
            "lon": round(destLon, 7),
            "color": {"r": int(r), "g": int(g), "b": int(b)},
            "color_max": {"r": int(r_max), "g": int(g_max), "b": int(b_max)},
            "dbz_nearest": int(dbz),
            "dbz_max": int(dbz_max),
            "mmh_nearest": float(precipitation),
            "mmh_max": float(DBZLUT.get(dbz_max, 0.0)),
            "computation_ms": delta,
        })
    else:
        response["rain"] = False
        response["computation_ms"] = delta

    return response
    
# Pokud aplikace poběží jako server, tato funkce nastartuje automatické stahování snímku každých pět minut.
# Volíme časy vždy minutu po celé pětiminutovce, tou dobou už totiž bude snímek s velkou pravděpodobností připravený na serveru ČHMÚ.
def initScheduler():
    for m in range(1, 60, 5):  # 1., 6., 11., 16. minuta atd.
        schedule.every().hour.at(f":{m:02d}").do(getRadarImage)

# Pomocná funkce pro časovač plánovače výše. Tato funkce se volá každou sekundu.
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
        
        Cim vyssi hodnota max_distance, tim vetsi vysek obrazu se prohledava. Vektorove vyhledavani zvlada i hledani pres cely radarovy snimek v radu jednotek milisekund. Pevny horni limit zde neni - slice se automaticky orezne na velikost obrazu.
    """
    return string
    
# Třída HTTP požadavku /, ze kterého zavoláme rainProcessor a vrátíme HTTP klientovi odpověď
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
            maxDistance = int(maxDistance) if maxDistance is not None else DEFAULT_MAX_DISTANCE

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
    # Stáhneme čerstvý snímek (nebo podle konfigurace načteme ze souboru)
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
    # Toto je ukázka pro bezplatné certifikáty Let's Encrypt, doménu radar.kloboukuv.cloud a výchozí cesty k certifikátům LE na Linuxu 
    if HTTPS:
        try:
            https = tornado.httpserver.HTTPServer(
                web,
                ssl_options = {
                    "certfile": "/etc/letsencrypt/live/radar.kloboukuv.cloud/fullchain.pem",
                    "keyfile": "/etc/letsencrypt/live/radar.kloboukuv.cloud/privkey.pem"
                }
            )
            # HTTPS poslouchá na standardním TCP portu 443
            https.listen(443)
        except:
            print("Nepodařilo se nastartovat HTTPS sezení. Zkontroluj, že máš funkční certifikáty.\nServer poběží nešifrovaný!")

    # Nastartujeme pomocný časovač secondTick pro řízení plánovače, který se bude volat systémovou smyčkou každých 1000 ms
    secondTickTask = tornado.ioloop.PeriodicCallback(secondTick, 1000)
    secondTickTask.start()

    # Nastartujeme systémovou smyčku, která drží server při životě
    printl("Startuji aplikační smyčku...")
    tornado.ioloop.IOLoop.current().start()
