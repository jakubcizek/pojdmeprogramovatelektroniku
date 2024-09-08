#Instalace knihovny: pip install astral
#Dokumentace: https://astral.readthedocs.io/en/latest/
from astral import Observer
from astral import moon
from astral.moon import moonrise
from datetime import date
pozorovatel = Observer(
    latitude = 49.2323311, 
    longitude = 16.5829453, 
    elevation = 250
)
vychod = moonrise(
    pozorovatel, 
    date = date.today(), 
    tzinfo = "Europe/Prague"
)
faze = moon.phase(date.today())
vychod = vychod.strftime("%d.%m. v %H:%M:%S")
print(f"Měsíc vychází {vychod}")
print(f"Fáze měsíce: {faze:.2f}")
