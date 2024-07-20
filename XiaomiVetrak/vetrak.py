# Knihovna pro komunikaci s elektronikou Xioami
# python-miio.readthedocs.io/
from miio import FanMiot, DeviceException
from time import sleep

# NASTAVENÍ PŘÍSTUPOVÝCH ÚDAJŮ K VĚTRÁKU
# Tyto informace získáte příkazem: miiocli cloud
ip = "lan-ip-adresa-vetraku"
token = "token-vetraku"

vetrak = FanMiot(ip, token)
print("Zjišťuji informace o větráku:")
try:
    status = vetrak.status()
    for parametr, hodnota in status.data.items():
        print(f"{parametr}: {hodnota}")

    print("\nZapínám větrák")
    vetrak.set_property("power", True)
    sleep(10)
    print("Vypínám větrák")
    vetrak.set_property("power", False)
except DeviceException as chyba:
    print(f"Nedaaří se mi ovládat větrák: {chyba}")
