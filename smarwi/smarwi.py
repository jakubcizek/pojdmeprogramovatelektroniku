# Ukazka zdrojoveho kodu pro ovladani Smarwi v Pythonu
# Pro potreby clank uv casopisu Computer, cerven 2024

import requests # https://requests.readthedocs.io/en/latest/
import sys

# Identifikace zarizeni, viz https://vektiva.gitlab.io/vektivadocs/api/api.html
REMOTE_ID = ""
API_KEY = ""
DEVICE_ID = ""
POVELY = ["open", "close"] # Povolene povely open a close pro otevreni/zavreni okna
URL = (
  "https://vektiva.online/api/"
  f"{REMOTE_ID}/"
  f"{API_KEY}/"
  f"{DEVICE_ID}/"
)

if len(sys.argv) == 2 \
  and sys.argv[1].lower() in POVELY:
  POVEL = sys.argv[1]
else:
  print(f"Použití skriptu:")
  for povel in POVELY:
    print(f"\tpython smarwi.py {povel}")
  exit()

r = requests.get(f"{URL}{POVEL}", timeout=10)
if r.status_code == 200:
  odpoved = r.json()
  if odpoved["code"] == 0 \
    and odpoved["message"] == "OK":
    print(f"Povel {POVEL} úspěšně proveden")
  else:
    print("Asi se to nepovedlo")
    print(f"Odpověď ze serveru: {odpoved}")
else:
  print("Chyba spojení se serverem")
  print(f"HTTP kod: {r.status_code}")
