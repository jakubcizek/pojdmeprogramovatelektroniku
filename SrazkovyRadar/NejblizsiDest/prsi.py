import requests
from PIL import Image, ImageDraw
from time import sleep
from io import BytesIO
from datetime import datetime, timedelta, timezone

DBZLUT = {
    4: 0.1, 8: 0.325, 12: 0.55, 16: 0.775,
    20: 1.0, 24: 3.25, 28: 5.5, 32: 7.75,
    36: 10.0, 40: 28.0, 44: 46.0, 48: 64.0,
    52: 82.0, 56: 100.0, 60: 100.0
}

def barvaNaDbz(r, g, b):
    lut = {
        (56, 0, 112): 4, (48, 0, 168): 8,
        (1, 1, 246): 12, (0, 108, 192): 16,
        (0, 160, 0): 20, (0, 188, 0): 24,
        (52, 216, 0): 28, (152, 215, 1): 32,
        (219, 215, 1): 36, (252, 176, 0): 40,
        (252, 132, 0): 44, (252, 88, 0): 48,
        (252, 0, 0): 52, (153, 2, 2): 56,
        (252, 252, 252): 60
    }
    dbz = 0
    nejkratsi_vzdalenost = float("inf")
    for (cr, cg, cb), _dbz in lut.items():
        vzdalenost = (r - cr) ** 2 + (g - cg) ** 2 + (b - cb) ** 2
        if vzdalenost < nejkratsi_vzdalenost:
            nejkratsi_vzdalenost = vzdalenost
            dbz = _dbz
    return dbz

radar = None
datum_txt = ""

datum = datetime.now(timezone.utc)
datum = datum.replace(
    minute=datum.minute - (datum.minute % 5), 
    second=0, 
    microsecond=0
)

pokusy = 3

while pokusy > 0:
    datum_txt = datum.strftime("%Y%m%d.%H%M")
    url = f"https://www.chmi.cz/files/portal/docs/meteo/rad/inca-cz/data/czrad-z_cappi020/pacz2gmaps3.z_cappi020.{datum_txt}.0.png"
    r = requests.get(url)
    if r.status_code != 200:
        print("Soubor neexistuje, zkusím stáhnout o 5 minut starší")
        datum -= timedelta(minutes=5)
        pokusy -= 1
        sleep(.5)
    else:
        radar = Image.open(BytesIO(r.content))
        radar = radar.convert("RGBA")  
        break

if radar == None:
    print("Nepodařilo se stáhnout snímek, končím")
else:
    print(f"Snímek s časovou značkou {datum_txt} je stažený")

    pixely = radar.load()

    X = 140
    Y = 140

    r, g, b, alfa = pixely[X, Y]

    if alfa > 0 and (r | g | b) != 0:
        print(f"Na pozici {X}x{Y} prší:")
        print(f"\tBarva deště: R={r}, G={g}, B={b}")
        print(f"\tIntenzita odrazu: {barvaNaDbz(r,g,b)} dBz")
        print(f"\tUhrn srážek: {DBZLUT[barvaNaDbz(r,g,b)]} mm/h")

        znacka = [X - 10, Y - 10, X + 10, Y + 10]
        platno = ImageDraw.Draw(radar, "RGBA")
        platno.rectangle(znacka, outline=(255, 0, 0), fill=None, width=2)
        radar.save(f"radar.png")
    else:
        print(f"Na pozici {X}x{Y} neprší!")
