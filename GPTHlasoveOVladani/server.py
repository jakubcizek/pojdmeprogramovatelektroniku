# Knihovna Tornado pro spuštění HTTP serveru
# https://www.tornadoweb.org/en/stable/
import tornado.ioloop
import tornado.web
import tornado.httpserver

# Knihovna OpenAI pro práci s OpenAI Platform
# https://platform.openai.com/docs/libraries
import openai

# Knihovna Requests pro HTTP klienta
# https://requests.readthedocs.io/en/latest/
import requests

# Knihovna pywizlight pro komunikaci s chytrými žárovkami Wiz
# https://github.com/sbidy/pywizlight
from pywizlight import wizlight, PilotBuilder

# Knihovna PyP100 pro komunikaci s chytrými zásuvkami Tp-Link Tapo
# https://github.com/sbidy/pywizlight
from PyP100 import PyP110

import asyncio # Knihovna pro asycnrhonní operace
import json # Knihoivna pro práci s JSON

# Konfigurace spojení s OpenAI Platformou
# Vyplňte vlastní klíč API (https://platform.openai.com/account/api-keys)
# Ceník tokenů platný k 18.6.2023 (https://openai.com/pricing)
configuration = {
    "apiKey": "sk-bobikbobikbobikbobikbobikbobikbobik",
    "apiModel": "gpt-3.5-turbo-0613",
    "apiPriceInput1k": 0.0015, 
    "apiPriceOutput1k": 0.002 
}

# Databáze s chytrými zařízením iv síti, které budeme ovldát
# Pro AI GPT 3.5 je důležitá položka name, což je unikátní identifikátor,
# který by měl vyjadřovat skutečnou podstatu zařízení, aby GPT-3.5 mohl odhadnout,
# že to je třeba světlo/lampa, větrák atp.
# Pro náš program pak slouží vše ostatní
devices = [
    {
        "name": "lampa-dilna",
        "friendly": "Lampa v dílně",
        "type": "wizz-bulb",
        "ip": "172.17.16.171"
    },
    {
        "name": "vetrak",
        "friendly": "Redakční větrák",
        "type": "tapo-plug-p110",
        "ip": "172.17.16.173",
        "username": "login k uctu TP-Link",
        "password": "heslo k uctu TP-Link"
    },
    {
        "name": "klimatizace",
        "friendly": "Redakční klimatizace",
        "type": "esp32-klima-http-driver",
        "hostname": "klima.local"
    }
]


# Asynchronní funkce pro práci s žárovkou Wiz
async def setWiz(ip, action, data):
    device = wizlight(ip)
    if action == "on":
        await device.turn_on()
    elif action == "off":
        await device.turn_off()
    elif action == "color":
        await device.turn_on(PilotBuilder(rgb = (data["r"], data["g"], data["b"])))

# Funkce pro práci se zásuvckou Tapo P110
def setTapo(device, action):
    plug = PyP110.P110(device["ip"], device["username"], device["password"])
    plug.handshake()
    plug.login()
    if action == "on":
        plug.turnOn()
    elif action == "off":
        plug.turnOff()

# Funkce pro práci s HTTP serverem naší chytré redakční klimatizace
def setKlimatizace(hostname, action):
    if action == "on":
        requests.get(f"http://{hostname}/prikaz/zapni")
    elif action == "off":
        requests.get(f"http://{hostname}/prikaz/vypni")

# Funkce pro vytvoření JSON struktury s chybou
def createError(text):
    jsonObj = {
        "return": False,
        "text": text
    }
    return json.dumps(jsonObj)

# Funkce pro převod vstupních/výstupních tokenů na USD dle ceníku v konfiguraci skriptu
def tokens2USD(tokens, input=True):
    if input:
        return (tokens / 1000.0) * configuration["apiPriceInput1k"]
    else:
        return (tokens / 1000.0) * configuration["apiPriceOutput1k"]


# Třida Smarthome pro zpracování HTTP GET/POST/OPTIONS spojení se serverem pomocí knihovny Tornado
class Smarthome(tornado.web.RequestHandler):
    # Výchozí HTTP hlavičky nastavující co nejvolnější politiku CORS (https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS)
    # Z Javascriptu se tak můžeme spojit se serverem, i když poběží na jiné doméně
    def set_default_headers(self):
        self.set_header("Access-Control-Allow-Origin", "*")
        self.set_header("Access-Control-Allow-Headers", "Content-Type, Access-Control-Allow-Headers")
        self.set_header("Access-Control-Allow-Methods", "*")

    # Pro plně funkční CORS je třeba ještě obslužná funkce pro HTTP OPTIONS
    def options(self):
        self.set_status(204)
        self.finish()

    # Tato funkce se zavolá při HTTP POST požadavku
    def post(self):
        self.set_header("Content-Type", "application/json")
        # Získáme části url formátované jako /část/část/čast...
        path = self.request.path.split("/")
        del path[0]
        if len(path) > 1:
            # Pokud má URL formát ../speech/,
            # zpracujeme příchozí data
            if path[1] == "speech":
                # Přečtemě tělo POST jako JSON
                speech = json.loads(self.request.body)
                # V doručeném JSONu je klíč text, jehož obsah
                # pošleme ke zpracování do OpenAI
                # Společně s textem pošleme ve formátu JSON i onu obsáhlou deklaraci
                # podporovaných funkcí, které může GPT-3.5 použít pro převod přirozené řeči na strojovou instrukci
                if "text" in speech:
                    response = openai.ChatCompletion.create(
                        model = configuration["apiModel"],
                        messages = [
                            {
                                "role": "user", 
                                "content": speech["text"]
                            }
                        ],
                        functions = [
                            {
                                "name": "set-switch",
                                "description": "Set switch of device. We can switch on or switch off device",
                                "parameters": {
                                    "type": "object",
                                    "properties": {
                                        "id":{
                                            "type": "string",
                                            "description": "Unique name of device",
                                            "enum": [device["name"] for device in devices]
                                        },
                                        "state":{
                                            "type": "string",
                                            "description": "State of switch",
                                            "enum": ["on", "off"]
                                        }
                                    },
                                    "required": ["id", "state"]
                                }
                            },
                            {
                                "name": "set-color",
                                "description": "Set RGB color of device",
                                "parameters": {
                                    "type": "object",
                                    "properties": {
                                        "id":{
                                            "type": "string",
                                            "description": "Unique name of device",
                                            "enum": [device["name"] for device in devices]
                                        },
                                        "r":{
                                            "type": "integer",
                                            "description": "Red channel of RGB",
                                        },
                                        "g":{
                                            "type": "integer",
                                            "description": "Green channel of RGB",
                                        },
                                        "b":{
                                            "type": "integer",
                                            "description": "Blue channel of RGB",
                                        }
                                    },
                                    "required": ["id", "r", "g", "b"]
                                }
                            }
                        ]
                    )

                    # V TUTO CHVÍLI MÁME NĚJAKOU ODPOVĚĎ OD OPENAI

                    # Proměnné s odpovědí a spotřebovanými tokeny pro vstup a výstup
                    messageObj = response["choices"][0]["message"]
                    tokensInput = response["usage"]["prompt_tokens"]
                    tokensOutput = response["usage"]["completion_tokens"]
      
                    # Pokud je v odpovědi klíč function_cal, GTP-3.5 skutečně použil některou z předdefinovaných funkcí
                    if "function_call" in messageObj:
                        # Připravíme si proměnnou pro textovou hlášku, kterou pošleme zpět klientovi (HTTP prohlížeči) 
                        text = ""
                        # Proměnná s názvem funkce, kterou GPT-3.5 zvolil
                        function = messageObj["function_call"]["name"] 
                        # JSON s argumenty funkce
                        arguments = json.loads(messageObj["function_call"]["arguments"])

                        # Pokud se jedná o funkci set-switch
                        if function == "set-switch":
                            # Získáme id/jméno zařízení a stav, na který jej máme nastavit (on/off)
                            id = arguments["id"]
                            state = arguments["state"]
                            # Z JSON databáze dostupných zařízení, kterou jsme si vytvořili na začátku,
                            # vrátíme to zařízení, které odpovídá identifikátoru
                            device = next((device for device in devices if device["name"] == id), None)

                            # Zkonstruujeme textovou hlášku pro klienta (webový prohlížeč)
                            if state == "on":
                                text = f"Zapnul jsem zařízení {device['friendly']}"
                            else:
                                text = f"Vypnul jsem zařízení {device['friendly']}"

                            # Pokud se jedná o zařízení typu Wizz, zpracujeme jej asynchronní funkcí setWiz
                            if device["type"] == "wizz-bulb":
                                asyncio.create_task(setWiz(device["ip"], state, None))

                            # Pokud se jedná o zařízení typu naší zbastlené klimatizace, zpracujeme jej funkcí setKlimatizace
                            elif device["type"] == "esp32-klima-http-driver":
                                setKlimatizace(device["hostname"], state)

                            # Pokud se jedná o zařízení typu Tp-Link Tapo, zpracujeme jej funkcí setTapo
                            elif device["type"] == "tapo-plug-p110":
                                setTapo(device, state)

                            # V opačném případě vrátíme hlášku, že dané zaízení nepodporuje on/off
                            else:
                               text = f"Zařízení {device['friendly']} nepodporuje zapínání a vypínání" 

                        # Pokud se jedná o funkci set-color, podobným způsobem zpracujeme argumenty funcke vytvořené GPT-3.5
                        # a změníme barvu žárovky Wiz. Ostatní zařízení v síti nepodporují nastavení barvy, čili pokud se nejedná
                        # o Wiz, vytvoříme opět hlášku o nepodporovaném typu 
                        elif function == "set-color":
                            id = arguments["id"]
                            r = arguments["r"]
                            g = arguments["g"]
                            b = arguments["b"]
                            device = next((device for device in devices if device["name"] == id), None)

                            text = f"Nastavil jsem zařízení {device['friendly']} na barvu RGB ({r}, {g}, {b})"
                
                            if device["type"] == "wizz-bulb":
                                asyncio.create_task(setWiz(device["ip"], "color", {"r":r, "g": g, "b": b}))
                            else:
                                text = f"Zařízení {device['friendly']} nepodporuje nastavení barvy" 

                        # Vytvoříme kompletní návratový JSON včetně statistiky spotřebovaných tokenů
                        returnJson = {
                            "return": True,
                            "text": text,
                            "tokensInput": tokensInput,
                            "tokensOutput": tokensOutput,
                            "tokensInputPrice": tokens2USD(tokensInput),
                            "tokensOutputPrice": tokens2USD(tokensOutput),
                        }

                        # Odešleme JSON s odpovědí připojenému klientu (webovému prohlížeči)
                        self.write(json.dumps(returnJson))

                    # Pokud GPT-3.5 neodkázal vstupní text převést na žádnou funkci,
                    # v návratovém JSONu použijeme jeho odpověď v přirozené řeči. Může vás
                    # v ní třeba žádat o zpřesnění dotazu, pokud si neníá jistý apod.   
                    else:
                        returnJson = {
                            "return": True,
                            "text": messageObj["content"],
                            "tokensInput": tokensInput,
                            "tokensOutput": tokensOutput,
                            "tokensInputPrice": tokens2USD(tokensInput),
                            "tokensOutputPrice": tokens2USD(tokensOutput),
                        }
                        self.write(json.dumps(returnJson))
                else:
                    self.write(createError("Špatný formát přepisu hlasu na text"))
            else:
                self.write(createError("Špatný formát URL"))
        else:
            self.write(createError("Špatný formát URL"))

    # Náš HTTP server očekává od klientů data v těle dotazu HTTP POST,
    # takže na běžný HTTP GET odpoví chybovou hláškou v prosrtém textu
    # Proč? Text z převodníku hlasu může být dlouhý a necheme jej posílat přímo v URL
    # jako běžný URL argument
    def get(self):
        self.set_header("Content-Type", "text/plain")
        self.write(createError("Smarthome API je dostupné jen skrze HTTP POST!"))


# **** Začátek programu ****
if __name__ == "__main__":
    # Nastavíme knihovně OpenAI náš klíč 
    openai.api_key = configuration["apiKey"]

    # Spustíme HTTP server na standardním TCP portu 80
    # Všechny URL začínající na /smarthome zpracujeme v třídě Smarthome
    web = tornado.web.Application([
        (r'/smarthome/?.*', Smarthome)
    ])
    web.listen(80)

    # Nastartujeme nekonečnou asynchronní aplikační smyčku,
    # jinak by program zase okamžitě skončil. Od této chvíle
    # HTTP server poslouchá a čeká na klienty
    # Skript můžeme zabít třeba stiskem CTRL+C 
    print("Startuji aplikační smyčku")
    tornado.ioloop.IOLoop.current().start()
