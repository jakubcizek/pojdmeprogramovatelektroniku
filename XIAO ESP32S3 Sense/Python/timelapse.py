import cv2 # Knihovna OpenCV
import glob # Knihovna pro získání seznamu obrázků v adresáři
from PIL import ImageFont, ImageDraw, Image # Knihovna PIL/Pillow pro práci s bitmapami
import numpy as np # Knihovna Numpy pro práci s kompelxními poli

# Adresář s fotkami
adresar = "TL018"

# Získej seznam cest k obrázkům podle filtru
# Soubory, které získáme, musejí mít příponu *.JPG
obrazky = glob.glob(f"{adresar}/*.JPG")

# Název videa
video = "časosběr.mp4"

# FPS videa
fps = 10

# Rozměry videa
sirka = 1200
vyska = 900
 
# Budeme používat kodek H264 pro MP4
kodek = cv2.VideoWriter_fourcc(*'mp4v')
 
# Nastavíme kodé=r videa
koder = cv2.VideoWriter(video, kodek, fps, (sirka, vyska))

# Font písma
font = ImageFont.truetype("AlfaSlabOne-Regular.ttf", 90) 

# Projdeme obrázek po obrázku
for obrazek in obrazky:
    print(f"Zpracovávám obrázek {obrazek}", end="\r")
    # Načteme obrázek pomocí OpenCV
    snimek = cv2.imread(obrazek)
    # Změníme velikost obrázku na rozměry videa
    snimek = cv2.resize(snimek, (sirka, vyska))
    # Převedeme obrázek na formát PIL/Pillow pro práci s grafikou
    bitmapa = Image.fromarray(cv2.cvtColor(snimek, cv2.COLOR_BGR2RGB))
    # Vytvoříme kreslící plátno
    platno = ImageDraw.Draw(bitmapa)
    # Naše texty na třech řádících. Mohou to být GPS údaje z dodatečného souboru aj.
    prvni_radek = "Text na prvním řádku"
    druhy_radek = "Text na druhém řádku"
    treti_radek = "Text na třetím řádku"
    # Nakrelsíme řádky do plátna
    # Každý řádek vje bílý s mírně posunutou černou kopií, která slouží jako efekt stínu 
    platno.text((20, vyska-310), prvni_radek, font=font, fill=(0,0,0))
    platno.text((10, vyska-320), prvni_radek, font=font, fill=(255,255,255))
    platno.text((20, vyska-210), druhy_radek, font=font, fill=(0,0,0))
    platno.text((10, vyska-220), druhy_radek, font=font, fill=(255,255,255))
    platno.text((20, vyska-110), treti_radek, font=font, fill=(0,0,0))
    platno.text((10, vyska-120), treti_radek, font=font, fill=(255,255,255))
    # Převedeme bitmapu PIL/Pillow zpět na formát BGR pole Numpy, se kterým pracuje OpenCV
    snimek = cv2.cvtColor(np.array(bitmapa), cv2.COLOR_RGB2BGR)
    # Zobrazíme snímek v okně
    cv2.imshow("Náhled videa", snimek)
    cv2.waitKey(1)
    # Pošleme snímek do H264 koderu
    koder.write(snimek)

# Zavřeme kodér
koder.release()
# Zavřeme okna
cv2.destroyAllWindows()
print("")