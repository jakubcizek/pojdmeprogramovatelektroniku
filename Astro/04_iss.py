#Instalace: pip install sskyfield
#Dokumentace: https://rhodesmill.org/skyfield/
from skyfield.api import Topos, load
from datetime import datetime
import pytz
tle_adresa = "http://celestrak.com/NORAD/elements/stations.txt"
telesa = load.tle_file(tle_adresa)
jmena = {teleso.name: teleso for teleso in telesa}
iss = jmena["ISS (ZARYA)"]
pozorovatel = Topos(
    latitude_degrees = 49.2323311, 
    longitude_degrees = 16.5829453, 
    elevation_m = 250
)
mistni_cas = datetime.now(pytz.timezone("Europe/Prague"))
ts = load.timescale()
ts_mistni_cas = ts.utc(mistni_cas)
rozdil = iss - pozorovatel
topocentricky = rozdil.at(ts_mistni_cas)
vyska, azimut, _ = topocentricky.altaz()
print(f"ISS: {vyska.degrees:.2f}° nad obzorem a {azimut.degrees:.2f}° od severu (azimut)")
