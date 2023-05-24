from PIL import Image
import requests

# Paleta pro 7barevny ACeP /  Waveshare e-inky
paleta = [
    (  0,   0,   0), # CERNA,    eink kod: 0x0
    (255, 255, 255), # BILA,     eink kod: 0x1
    (  0, 255,   0), # ZELENA,   eink kod: 0x2
    (  0,   0, 255), # MODRA,    eink kod: 0x3
    (255,   0,   0), # CERVENA,  eink kod: 0x4
    (255, 255,   0), # ZLUTA,    eink kod: 0x5
    (255, 128,   0)  # ORANZOVA, eink kod: 0x6
]

bitmapa = Image.new("RGB", (640, 400))

r = requests.get("https://oracle-ams.kloboukuv.cloud/radarmapa/?chcu=snimek_rle.bin")

x = 0
y = 0

stazeno = len(r.content)
data = r.content

print(f"Stazene bajty: {stazeno}")
print("Dekomprimuji a kreslim...")
# RLE dekomprese, 8bit pocitadlo
# Posloupnost bajtu: pocet, hodnota_bajtu, pocet, hodnota_bajtu...
for i in range(0, stazeno-1, 2):
     pocet = data[i]
     bajt = data[i+1] 
     for i2 in range(pocet):
        # 4bit kodovani s paletou; 1 bajt obsahuje 2
        # po sobe jdouci pixely s indexem palety
        px0 = bajt >> 4
        px1 = bajt & 0x0f
        bitmapa.putpixel((x,y), paleta[px0])
        x += 1
        if x == 640:
            x = 0
            y += 1
        bitmapa.putpixel((x,y), paleta[px1])
        x += 1
        if x == 640:
            x = 0
            y += 1

bitmapa.show()
