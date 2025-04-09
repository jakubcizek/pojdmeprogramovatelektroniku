from datetime import datetime # Práce s datem a časem
import json, os # Práce se soubory a JSON
import tornado.web # Webovy server, https://www.tornadoweb.org
import tornado.ioloop 
import requests # HTTP  klient, hhttps://requests.readthedocs.io/

# Globální slovník pro uložení posledních údajů z teploměrů
thermometers = {}

# Nastavení parametrů pro odesílání vlastních hodnot na službu Zivyobraz.eu
zivyobraz = True # Pokud True, posílání je povoleno
zivyobraz_params_template = {"import_key": "tvujklic"} # Klíč pro import dat, viz https://wiki.zivyobraz.eu/doku.php?id=portal:hodnoty

# Při HTTP GET dotazu /unixtime vrátí aktuální čas jako unixový čas v JSON formátu
# ESP32 zavolá po svém spuštění a nastaví si čas
# Numerický unixtime používáme pro jednoduché zpracování na čipu ESP32
# Pozor, v unixovém čase je čas UTC, takže na čipu ESP32 je případně třeba připočíst časové pásmo (pro jendoduchost kódu neřešíme)
class HttpUnixtime(tornado.web.RequestHandler):
    def get(self):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        self.write({"return": True, "unixtime": int(datetime.now().timestamp())})

# Obsluha HTTP GET a POST dotazu /teplomery
class HttpTeplomery(tornado.web.RequestHandler):
    # Při HTTP GET dotazu /teplomery vrátí HTML stránku s posledními údaji teploměrů
    def get(self):
        html = "<html><head><meta charset='UTF-8'><title>Teplomery</title></head><body>"
        html += "<h1>Poslední údaje teploměrů</h1>"
        html += "<table border='1' cellspacing='0' cellpadding='5'>"
        html += "<tr><th>Jméno</th><th>MAC</th><th>Teplota (°C)</th><th>Vlhkost (%)</th><th>Baterie (%)</th><th>Napětí (mV)</th><th>Počítadlo</th><th>Čas</th></tr>"
        for key, data in thermometers.items():
            html += (
                f"<tr>"
                f"<td>{data.get('name','')}</td>"
                f"<td>{data.get('mac','')}</td>"
                f"<td>{data.get('temperature','')}</td>"
                f"<td>{data.get('humidity','')}</td>"
                f"<td>{data.get('battery_percent','')}</td>"
                f"<td>{data.get('battery_mv','')}</td>"
                f"<td>{data.get('counter','')}</td>"
                f"<td>{data.get('strdatetime','')}</td>"
                f"</tr>"
            )
        html += "</table></body></html>"
        self.set_header("Content-Type", "text/html; charset=UTF-8") 
        self.write(html)

    # Při HTTP POST dotazu /teplomery zpracovává JSON data a ukládá je do souboru
    # a do globálního slovníku thermometers
    # Odpovědí je opět JSON objket s unixovým časem, takže si ESP32 může seřídit čas
    # Numerický unixtime používáme pro jednoduché zpracování na čipu ESP32
    def post(self):
        self.set_header("Content-Type", "application/json; charset=UTF-8")
        try:
            data = json.loads(self.request.body.decode("utf-8"))
        except json.JSONDecodeError as e:
            self.set_status(400)
            self.write({"return": False, "error": "Invalid JSON"})
            return

        if not isinstance(data, list):
            self.set_status(400)
            self.write({"return": False, "error": "Expected JSON array"})
            return
        
        if zivyobraz:
            zivyobraz_params = zivyobraz_params_template.copy()

        # Zpracování každého teploměru v seznamu
        for thermometer in data:
            required_keys = {"name", "mac", "temperature", "humidity", "battery_percent", "battery_mv", "counter", "unixtime"}
            if not required_keys.issubset(thermometer):
                self.set_status(400)
                self.write({"return": False, "error": f"Missing keys in: {thermometer}"})
                return
            
            name = thermometer["name"]
            thermometer["strdatetime"] = datetime.fromtimestamp(thermometer["unixtime"]).strftime("%H:%M:%S %d.%m. %Y")
            thermometers[name] = {k: thermometer[k] for k in ["name", "mac", "temperature", "humidity", "battery_percent", "battery_mv", "counter", "strdatetime"]}
            
            # Uložení do CSV souboru
            os.makedirs("data", exist_ok=True)
            with open(f"data/{name}.csv", "a") as f:
                f.write(";".join(str(thermometer[k]) for k in ["strdatetime", "temperature", "humidity", "battery_percent", "battery_mv", "counter"]) + "\n")

            if zivyobraz:
                zivyobraz_params[name] = thermometer["temperature"]

        self.write({"return": True, "unixtime": int(datetime.now().timestamp())})

        if zivyobraz:
            requests.get("https://in.zivyobraz.eu/", params=zivyobraz_params)


# Faktický začátek programu
if __name__ == "__main__":

    # Nastavení cest serveru, na kterých poslouchá,
    # portu 80 a spuštění nekonečné smyčky
    # Standardni port 80 pro HTTP vyžaduje spusštění s adekvátními právy
    server = tornado.web.Application([
        (r"/teplomery/?", HttpTeplomery),
	    (r"/unixtime/?", HttpUnixtime)
    ])
    server.listen(80)
    tornado.ioloop.IOLoop.current().start()
