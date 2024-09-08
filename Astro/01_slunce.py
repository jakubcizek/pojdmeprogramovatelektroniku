#Instalace knihovny: pip install astral
#Dokumentace: https://astral.readthedocs.io/en/latest/
from astral import Observer
from astral.sun import sun
from datetime import date
pozorovatel = Observer(
    latitude = 49.2323311, 
    longitude = 16.5829453, 
    elevation = 250
)
slunce = sun(
    pozorovatel, 
    date = date.today(), 
    tzinfo = "Europe/Prague"
)
vychod = slunce["sunrise"].strftime("%H:%M:%S")
zapad = slunce["sunset"].strftime("%H:%M:%S")
print(f"Slunce dnes vychází v {vychod}")
print(f"Slunce dnes zapadá v {zapad}")
