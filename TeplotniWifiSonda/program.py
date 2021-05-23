import signal # Detekce ukonceni programu
import json # Prace s JSON
import requests # HTTP klient (https://docs.python-requests.org/en/master/)
import glob # Prochazeni adresaru
import time # Prace s casem}
import schedule # Jednoduche planovani uloh (https://schedule.readthedocs.io/en/stable/)
import threading # Prace s vlakny
import os # Prikazova radka
import re # Regularni vyrazy
import sys # Ukonceni programu
from gpiozero import LED # Prace s GPIO piny (https://github.com/gpiozero/gpiozero)

# Tuto funkci zavolame, pokud program silou ukoncime
# Bezpecne vypne RGB LED/GPIO periferie
def uklid_pri_vypnuti(sig, fr):
    print("Vypinam RGB LED...")
    cervena.close()
    zelena.close()
    modra.close()
    print("Ukoncuji program...")
    smycka = False
    sys.exit()

# Funkce pro spusteni jine funkce v separatnim asynchronnim vlaknu, ktere neblokuje hlavni proud programu
def beh_ve_vlakne(funkce):
    vlakno = threading.Thread(target=funkce)
    vlakno.start()

# Funkce pro ziskani IP adresy
def ziskej_ip(interface):
    radek = os.popen("ip a show " + interface + "| grep inet").read().split("\n")[0].strip()
    if not interface in radek:
            return None
    else:
        adresy = re.findall(r"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})", radek)
        return adresy[0]

# Funcke pro ziskani teplot z teplomeru a jejich odeslani na web
def mereni():
    teplomery = glob.glob(zarizeni + "28*") # Ziskej seznam adresaru teplomeru, ktere odpovidaji vyrazu /sys/bus/w1/devices/28*
    if len(teplomery) == 0: # Pokud jsem zadne teplomery nenasel, rozsvitim cervenou chybovou LED
        modra.off()
        cervena.on()
    else: # Pokud jsem nejake nasel, rozsvitim zelenou LED signalizujici zacatek mereni
        modra.off()
        cervena.off()
        zelena.on()
        print(f"Nalezeno teplomeru: {len(teplomery)}")
        url_parametr = "?"
        for index, teplomer in enumerate(teplomery): # Projdu slozky teplomeru a otevru v nich soubor temperture
            soubor = open(teplomer + "/temperature")
            text = soubor.read() # Precti obsah souboru
            soubor.close()
            teplota = float(text) / 1000.0 # Podel obsah 1000 a ziskas teplotu
            print(f"Teplota cidla {teplomer}: {teplota} °C")
            url_parametr += "t" + str(index) + "=" + str(teplota) + "&" # Pridej teplotu do URL parametru ve formatu t0=YY.YYYY&t1=ZZ.ZZZ...
        url_parametr = url_parametr[:-1]
        print(f"Posílám data na web...")
        http = requests.get(url + url_parametr) # Vytvor HTTP GET dotaz s URL parametrem, ktery obsahuje ziskane hodnoty
        zelena.off()
        modra.on() # Zhasni zelenou LED a rozsvit vyckavaci modrou LED

# Zacatek hlavniho proudu programu
if __name__ == "__main__":

    cervena = LED(22) # Cervena LED na pinu 22
    zelena  = LED(23) # Zelena LED na pinu 23
    modra   = LED(27) # Modra LED na pinu 27

    smycka = True

    # Pokud dorazi signal preruseni/ukonceni programu, zpracuj funkci uklid_pri_vypnuti
    signal.signal(signal.SIGINT, uklid_pri_vypnuti)
    signal.signal(signal.SIGTERM, uklid_pri_vypnuti)

    modra.blink(.1, .1, None, True) # Rozblikej modrou LED v separatnim vlakne

    # Cekej, dokud neoverim, ze jsem pripojeny k Wi-Fi (dostal jsem od DHCP serveru IP adresu)
    ip = None
    while ip is None:
        ip = ziskej_ip("wlan0")
        if ip == None:
            print("Cekam na Wi-Fi...")
            time.sleep(1)
        else:
            print("Jsem pripojeny k Wi-Fi a mam IP adresu")

    modra.on() # Ukonci blikani a rozsvit vyckavaci modrou LED

    zarizeni = "/sys/bus/w1/devices/" # Adresar pripojenych 1-Wire zarizeni
    soubor = open("/boot/teplomer_konfigurace.txt") # Precti konfguraci programu jako JSON
    konfigurace = json.load(soubor)
    soubor.close()

    url = konfigurace["url"] # URL, na kterou budeme posilat data
    prodleva = konfigurace["prodleva"] # Prodlva mezi merenimi
    print(f"LAN IP adresa: {ip}")
    print(f"URL: {url}")
    print(f"Frekvence měření: {prodleva}")

    schedule.every(prodleva).seconds.do(beh_ve_vlakne, mereni) # Kazdou n-tou sekundu spoustej funkci mereni
    while smycka: # Smycka, ve ktere si planovac uloh kontroluje, jestli uz ma spustit naplanovanou ulohu
        schedule.run_pending()
        time.sleep(.1)