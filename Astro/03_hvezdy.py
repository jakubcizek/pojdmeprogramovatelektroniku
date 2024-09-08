#Instalace: pip install astropy
#Dokumentace: astropy.org
from astropy.coordinates import EarthLocation, AltAz, SkyCoord, get_body
from astropy import units
from astropy.time import Time
from datetime import datetime
import pytz

def ziskejVyskuAzimut(nazev, pozorovatel, cas):
    try:
       teleso = get_body(nazev.lower(), cas, pozorovatel)
    except:
        try:
           teleso = SkyCoord.from_name(nazev)
        except:
            raise ValueError(f"Neznámý objekt {nazev}") 
    
    projekce = AltAz(obstime = cas, location = pozorovatel)
    souradnice = teleso.transform_to(projekce)
    azimut = souradnice.az.degree
    vyska = souradnice.alt.degree
    return azimut, vyska
    
telesa = ["Arcturus", "Jupiter", "Mars"]
mistni_cas = datetime.now(pytz.timezone("Europe/Prague"))
cas = Time(mistni_cas)
pozorovatel = EarthLocation(
    lat = 49.2323311 * units.deg,
    lon = 16.5829453 * units.deg,
    height = 250 * units.m
)

for teleso in telesa:
    azimut, vyska = ziskejVyskuAzimut(teleso, pozorovatel, cas)
    print(f"{teleso}: {vyska:.2f}° nad obzorem a {azimut:.2f}° od severu (azimut)")
