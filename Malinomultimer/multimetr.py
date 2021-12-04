# Knihovna pro práci s piny GPIO (součást Raspberry Pi OS)
import RPi.GPIO as GPIO
# Knihovna pro práci se znakovým displejem
# Subor RPi_I2C_driver.py stáhnete z adresy:
# https://gist.github.com/DenisFromHR/cc863375a6e19dce359d#file-rpi_i2c_driver-py
# Umístěte jej do stejného adresáře jako náš program
import RPi_I2C_driver
# Knihovna pro práci s čipem INA219
# Soubor ina219.py stáhnete z adresy:
# https://www.waveshare.com/wiki/Current/Power_Monitor_HAT
# Umístěte jej do stejného adresáře jako náš program
import ina219
# Knihovna pro práci ee systémovými signaly
# Použijeme j i kzachycení stisku přerušení programu CTRL+C
import signal
# Knihovna pro práci s OS
# Použijeme ji ke spuštění systémového příkazu pro zjištění IP adresy
import os
# Knihovna pro práci s regulárnímu výrazy
import re
# Knihovna Tornado pro spuštění WWW serveru
# a hlavní asynchronní smyčky programu
from tornado import ioloop
from tornado import web
from tornado import websocket

# Nastavení typu číslování pinů na desce Raspberry Pi
GPIO.setmode(GPIO.BCM)

# Tlačítka budou na těchto pinech GPIO
tlacitkaPiny = [
    23, # tlacitko 1
    27, # tlacitko 2
    17, # tlacitko 3
    22  # tlacitko 4
]

# Uložené stavy tlačítek
tlacitkaStavy = [
    False,
    False,
    False,
    False
]

# Funkce, která se zpracuje po stisku některého z tlačítek
def stiskTlacitka(pin):
    for i, tlacitko in enumerate(tlacitkaPiny):
        if tlacitko == pin:
            print(f"Stisk tlacitka {i} (GPIO{pin})")
            if tlacitkaStavy[i] == True:
                tlacitkaStavy[i] = False
            else:
                tlacitkaStavy[i] = True
    # Pokud není ani jedno tlačítko ve stavu ZAPNUTO,
    # znovu zobraz úvodní zdravici
    if not True in tlacitkaStavy:
        vypisZdravici()

# Funkce pro vypsání zdravice na znakový displej 
def vypisZdravici():
    lcd.lcd_display_string(" Malinomultimer 1.0 ", 1)
    lcd.lcd_display_string("   *** Zive.cz ***  ", 2)
    lcd.lcd_display_string(f"IP: {ip}", 4)

# Měřící funkce, kterou bude hlavní smyčka spouštět každou sekundu
def mereni():
    # Smaž displej, pokud mám přepisovat displej
    if True in tlacitkaStavy:
        lcd.lcd_clear()
    # Projdi pole čidel INA219
    for i, multimetr in enumerate(multimetry):
        # Pokud je podle stavu tlačítka čidlo aktivní, přečti jeho hodnoty
        if tlacitkaStavy[i] == True:
            aktualizace = True
            napeti = multimetr.getBusVoltage_V()
            bocnik = multimetr.getShuntVoltage_mV() / 1000
            proud = multimetr.getCurrent_mA()            
            prikon = multimetr.getPower_W() 
            # Vypiš zprávu na displej
            zprava = f"{napeti:.2f}V {proud:.2f}mA {prikon:.2f}W"
            lcd.lcd_display_string(zprava, i+1)
            # Pokud je připojený websocketový HTTP klient,
            # pošli mu stejnou zprávu
            for wsKlient in wsKlienti:
                wsKlient.write_message(f"Čidlo {i}: {zprava}")

# Po příchodu systémové zprávy SIGINT
# zastav všechny smyčky, vypni podsvětlení displeje
# a nech korektně ukončit program
def sigint(sig, frame):
    print("Ukoncuji program...")
    mereniSmycka.stop()
    hlavniSmycka.stop()
    lcd.lcd_clear()
    lcd.backlight(0)


# Funkce pro získání IP adresy podle zadaného rozhraní
# Pro Wi-Fi se přečte odpověď systémového příkazu: ip a show wlan0 | grep inet 
def ziskejIP(rozhrani):
    radek = os.popen("ip a show " + rozhrani + "| grep inet").read().split("\n")[0].strip()
    if not rozhrani in radek:
        return None
    else:
        adresy = re.findall(r"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})", radek)
        return adresy[0]

# Třída pro HTTP GET
# Při zadání adresy / server vrátí soubor index.html
class HttpIndex(web.RequestHandler):
    def get(self):
        self.render("index.html")

# Třída pro WebSocket
# Při novém spojení si uložíme klienta do pole
class WebSock(websocket.WebSocketHandler):
    def open(self):
        wsKlienti.append(self)

    def on_close(self):
        wsKlienti.remove(self)


# TADY ZAČÍNÁ BĚH NAŠEHO PROGRAMU
if __name__== "__main__":
    print("Startuji Malinomultimer 1.0")

    # Vytvoř pole se seznamem připojených websocketových klientů
    wsKlienti = []

    # Získej IP adresu Raspberry Pi připojeného k síti skrze Wi-Fi
    ip = ziskejIP("wlan0")

    # Spusť funkci sigint po příchodu systémového přerušení SIGINT
    signal.signal(signal.SIGINT, sigint)

    # Nastartuj znakový displej a vypiš na ně zdravici
    print("Startuji znakový LCD displej")
    lcd = RPi_I2C_driver.lcd()
    lcd.backlight(8)
    vypisZdravici()

    # Nastav GPIO piny tlačítek na vstup
    # Aktivuj na těchto pinech interní pull-down (v rozpojeném stavu bude na pinu zaručeně logická 0)
    # Aktivuj na těchto pinech detektor náběžné hrany (při změně na logickou 1 se zavolá funkce stiskTlacítka)
    # Nastav bouncetime na 600 ms (zabrání možnému šumu při stisku mechanického tlačítka)
    print("Registruji stisky tlačítek")
    for tlacitko in tlacitkaPiny:
        GPIO.setup(tlacitko, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)
        GPIO.add_event_detect(tlacitko, GPIO.RISING, callback=stiskTlacitka, bouncetime=600)

    # Nastartuj multimetry INA219 na I2C adresách 0x40, 0x41, 0x42 a 0x43
    print("Startuji multimetry")
    multimetry = []
    multimetry.append(ina219.INA219(addr=0x40))
    multimetry.append(ina219.INA219(addr=0x41))
    multimetry.append(ina219.INA219(addr=0x42))
    multimetry.append(ina219.INA219(addr=0x43))

    # Zaregistruj HTTP GET požadavek / a WebScoket požadavek /websocket 
    print(f"Startuji HTTP/WEBSOCKET server na adrese {ip}:80")
    webserver = web.Application([
        (r'/', HttpIndex),
        (r'/websocket', WebSock)
    ])
    # Nastartuj HTTP/WebScoket server na TCP portu 80
    webserver.listen(80)

    # Vytvoř sekundární smyčku třídy Tornado, která se bude opakovat každých 1 000 ms
    # V této smyčce se bude měřit 
    print("Startuji měřící smyčkyu s frekvencí 1 Hz")
    mereniSmycka = ioloop.PeriodicCallback(mereni, 1000)
    mereniSmycka.start()

    # Nastartuj hlavní smyčku třídy Tornado, která bude vše řídit
    print("Startuji hlavní smyčku, pomodleme se!")
    print("Pro ukončení stiskněte CTRL+C")
    hlavniSmycka = ioloop.IOLoop.current()
    hlavniSmycka.start()

    # Nyní je program spuštěný a bude spuštěný tak dlouho,
    # dokud nestiskneme kombinaci CTRL+C, anebo jej zabijeme jiným způsobem
