# Knihovny HTTP serveru Tornado
# Je třeba ručně doinstalovat třeba příkazem pip install tornado
# https://www.tornadoweb.org/en/stable/
import tornado.ioloop
import tornado.web
import tornado.httpserver
import tornado.websocket

# Knihovna Requests pro HTTP komuniakci s OpenAI API
# Je třeba ručně doinstalovat třeba příkazem pip install requests
#https://requests.readthedocs.io/en/latest/
import requests

# Další pomocné a už vestavěné knihovny
# pro práci s JSON, časem a Base64
import json
from datetime import datetime
import base64

# API klíč pro přístup k OpenAI API
# Vygenerujete si ho zde: https://platform.openai.com/api-keys
# Nikdy jej nezveřejňujte, nebo vám ho někdo ukradne!
# Proto není součástí JS/HTMl frontendu, ale serverového backendu
api_key = ""

# Kvalita obrazové analýzy (low, high, auto)
# Ale pozor, ve statistice nákladů níže máme implementované jen low, což pro naše účely stačí
gpt4v_image_quality = "low" 

# Prompt, co má GPT-4 Vision s obrázkem vlastně dělat
# GPT-4 Vision je specializovaná verze GPT-4, takže prompt může obsahovat cokoliv
gpt4v_prompt = "Jsi asistent pro nevidomé. Popiš scénu na obrázku včetně barev a rozměrů. Popis začni slovy: 'Na obrázku je' i když bude rozmazaný apod."

# Rozlišení obrázku, který bude generovat DALL-E 3 (1024x1024, 	1024×1792, 1792×1024)
dalle3_image_size = "1024x1024" 

# Kvalita obrázku, který bude generovat DALL-E 3 (standard, hd)
dalle3_image_quality = "standard"

# HTTP hlavičky pro komunikaci s OpenAI API skrze HTTP POST
headers = {
    "Content-Type": "application/json",
    "Authorization": f"Bearer {api_key}"
}

# Struktura se statistikou nákladů
price_stats = {
    "gpt-4-vision-preview":{
        "images_usd": 0,
        "input": 0,
        "output": 0,
        "tokens_usd": 0
    },
    "dall-e-3":{
        "usd": 0
    }
}

# Struktura s ceníkem modelů GPT-4 Vision a DALL-E 3
# Platná k 27. listopadu 2023
# Podle těchto dat budeme počítat náklady
price = {
    "dall-e-3":{
        "standard":{
            "1024x1024": 0.04,
            "1024×1792": 0.08,
            "1792×1024": 0.08

        },
        "hd":{
            "1024x1024": 0.08,
            "1024×1792": 0.08,
            "1792×1024": 0.12
        }  
    },
    "gpt-4-vision-preview":{
        "input": 0.01,
        "output": 0.03,
        "low": 0.00085
    }
}

# Pomocná funkce pro vytvoření JSON obálky pro odpověď,
# kterou budeme posílat skrze websocket připojenému klientovi (webovému prohlížeči)
def createMessage(type, data):
    return json.dumps({
        "type": type, 
        "data": data
    })

def askGPT4V(prompt, image):
    # Tělo HTTP POST
    # Konfigurace v JSON, která obsahuje bajty obrázku jako Base64 text
    # a také povel (prompt), co má GPT-4 Vision s obrázkem vlastně dělat
    # Nechybí ani nastavení kvality, s jakou má GPT-4 Vision obrázke analyzovat (nízká) 
    payload = {
        "model": "gpt-4-vision-preview",
        "messages": [
            {
                "role": "user",
                "content": [
                    {
                        "type": "text",
                        "text": prompt
                    },
                    {
                        "type": "image_url",
                        "image_url": {
                            "url": image,
                            "detail": gpt4v_image_quality
                        }
                    }
                ]
            }
        ],
        "max_tokens": 200
    }
    # HTTP komunikace s OpenAI API    
    response = requests.post("https://api.openai.com/v1/chat/completions", headers=headers, json=payload)
    try:
        code = response.status_code 
        response = response.json()
        return code, response
    except:
        return -1, None
    
# Funkce pro komunikaci s modelem DALL-E 3 skrze OpenAI API a knihovnu Requests   
def askDalle(prompt):
    # Tělo HTTP POST
    # Konfigurace povelu v JSON, co má DALL-E vygenerovat a v jaké kvalitě a rozlišení
    payload = {
        "model": "dall-e-3",
        "size": dalle3_image_size,
        "n": 1,
        "quality": dalle3_image_quality,
        "prompt": prompt
    }
    # HTTP komunikace s OpenAI API
    response = requests.post("https://api.openai.com/v1/images/generations", headers=headers, json=payload)
    try:
        code = response.status_code 
        response = response.json()
        return code, response
    except:
        return -1, None


# Třída WebScoket se zpracuje při HTTP WebSOcket požadavku /ws
# Tady se skrývá veškerá logika komunikace s klientem a OpenAI API
# WebSocket používáme pro jeho obosuměrnou a trvale otevřenou komunikaci s klienty
class WebSocket(tornado.websocket.WebSocketHandler):
    # Při otevření nového spojení pošli zprávu se statistikou spotřebovaných tokenů
    # ať se při novém načtená strányk hned aktualizuje statisika v pravém horním rohu stránky
    def open(self):
        self.write_message(createMessage("price-stats", price_stats))

    # Při příchodu zprávy od klienta/webového prohlížeče
    def on_message(self, message):
        # Zprávy musejí mít formát JSON,
        # a tak převedeme text na objekt JSON
        message = json.loads(message)
        # Pokud se jedná o zprávu typu image,
        # načteme data obrázku a odešleme klientovi potvrzovací zprávu
        if message["type"] == "image":

            image_data = message["data"]
            self.write_message(createMessage("uploaded", "Obrázek úspěšně nahraný na server"))

        # Odelšeme obrázek do modelu GPT-4 Vision
        code, response = askGPT4V(gpt4v_prompt, image_data)
        # Pokud nastala chyba, odešleme klientovi chybové hlášení
        if code != 200:
            message = {
                "reason": "http-connection",
                "function": "askGPT4V",
                "code": code,
                "data": response
            }
            self.write_message(createMessage("error", message))
        # Pokud chyba nenastala, pošleme klientovi zprávu s odpovědí od GPT-4 Vision
        else:   
            self.write_message(createMessage("gpt-4-vision-preview", response))
            # Pokud jsou součástí zprávy informace o spotřebovaných tokenech,
            # aktualizujeme statistiku, kterou si vedeme po celo udobu běhu serveru
            # a mohli bychom ji případně ukládat do souboru jako JSON 
            if "usage" in response:
                price_stats["gpt-4-vision-preview"]["input"] += response["usage"]["prompt_tokens"]
                price_stats["gpt-4-vision-preview"]["output"] += response["usage"]["completion_tokens"]
                price_stats["gpt-4-vision-preview"]["images_usd"] += price["gpt-4-vision-preview"][gpt4v_image_quality]
                tokens_usd = (price_stats["gpt-4-vision-preview"]["input"]/1000.0) * price["gpt-4-vision-preview"]["input"]
                tokens_usd += (price_stats["gpt-4-vision-preview"]["output"]/1000.0) * price["gpt-4-vision-preview"]["output"]
                price_stats["gpt-4-vision-preview"]["tokens_usd"] = tokens_usd
                # Odešleme klientovi zprávu se statistikou
                self.write_message(createMessage("price-stats", price_stats))
            
            # Pokud odpověď obsahuje text, co je na obrázku, zvolíme první variantu (v praxi je zatím vždy jen jedna)
            if "choices" in response:
                prompt = response["choices"][0]["message"]["content"]
                # Odešleme popis obrázku jako povel ke generování grafiky pomocí DALL-E 3
                code, response = askDalle(prompt)
                # Pokud nastala chyba, odešleme klientovi chybové hlášení
                if code != 200:
                    message = {
                        "reason": "http-connection",
                        "function": "askDalle",
                        "code": code,
                        "data": response  
                    }
                    self.write_message(createMessage("error", message))
                # Pokud chyba nenastala, zpracujeme odpověď
                else:  
                    # Pokud je v odpovědi jSON parametr data, DALL-E 3 dokončil generování úspěšně
                    if "data" in response:
                        # V odpovědi je odkaz na obrázek, který je uložený v datacentru Microsoft Azure
                        image_url = response["data"][0]["url"]
                        # A také skutečný anglický prompt, podle kterého DALL-E 3 generoval obrázek
                        real_prompt = response["data"][0]["revised_prompt"]
                        # Aktualizujeme statistiku s cenou
                        price_stats["dall-e-3"]["usd"] += price["dall-e-3"][dalle3_image_quality][dalle3_image_size]
                        # Pokusíme se stáhnout obrázek ze serverů Microsoft Azure
                        response = requests.get(image_url)
                        # Pokud se to podařilo
                        if response.status_code == 200:  
                            # Uložíme si aktuální čas a datum
                            dt = datetime.today().strftime("%Y%m%d_%H%M%S")
                            dt_wide = datetime.today().strftime("%d.%m. %Y v %H:%M:%S")
                            # Uložíme vygenerovaný obrázek do složky images/
                            with open(f"images/generovany_{dt}.png", "wb") as imgfile:
                                imgfile.write(response.content)
                            # Uložíme původní obrázek do složky images/
                            with open(f"images/puvodni_{dt}.jpg", "wb") as imgfile:
                                encoded = image_data.split(",", 1)[1]
                                imgfile.write(base64.b64decode(encoded))
                            # Zakódujeme bajty obrázku jako Base64 text
                            # Vytvoříme JSON odpověď a tu odešleme klientovi
                            # jako websocketovou zprávu
                            image = base64.b64encode(response.content).decode("utf-8")
                            data = {
                                "data": f"data:image/png;base64,{image}",
                                "filename": f"images/generovany_{dt}.jpg",
                                "real_prompt": real_prompt,
                                "datetime": dt_wide
                            }
                            self.write_message(createMessage("dall-e-3", data))
                        # Pokud se to nepodařilo, vytvoříme JSON s chybou,
                        # která se vypíše v konzoli prohlížeče, takže můžeme zkoumat,
                        # kde se stala chyba
                        else:
                            message = {
                                "reason": "http-connection",
                                "function": f"WebSocket/on_message/requests.get({image_url})",
                                "code": response.status_code,
                                "data": response  
                            }
                            self.write_message(createMessage("error", message))

                    # Pokud v odpovědi nemáme data, vytvoříme JSON s chybou,
                    # která se vypíše v konzoli prohlížeče, takže můžeme zkoumat,
                    # kde se stala chyba
                    else:
                        message = {
                            "reason": "http-connection",
                            "function": f"WebSocket/on_message/if data in response:",
                            "code": response.status_code,
                            "data": response  
                        }
                        self.write_message(createMessage("error", message))

                self.write_message(createMessage("price-stats", price_stats))
                
# Třida Webpage se zpracuje při HTTP GET požadavku /
# Načte soubor app.html a vrátí klientům jeho obsah jako text/html
class Webpage(tornado.web.RequestHandler):
    def get(self):
        self.set_header("Content-Type", "text/html; charset=utf-8")
        with open("app.html", "r") as f:
            html = f.read()
            self.write(html)


# **** Začátek programu ****
if __name__ == "__main__":

    # Při HTTP požadavku /assets/* server vrátí odpovídající soubor z adresáře assets
    # Při HTTP požadavku / server zpracuje třídu Webpage
    # Při HTTP požadavku /ws server zpracuje třídu WebSocket 
    web = tornado.web.Application([
        (r"/assets/(.*)", tornado.web.StaticFileHandler, {"path": "assets/"}),
        (r"/", Webpage),
        (r"/ws", WebSocket),
    ])
    # Spustí se HTTP server na TCP portu 80
    web.listen(80)

    # Spustí se nekonečná smyčka aplikace,
    # ze které můžeme vystoupit stisknutím CTRL+C,
    # nebo přerušením procesu jiným způsobem
    print("Lokální webový server poslouchá na adrese http://localhost")
    print("Ukonči program stiskem CTRL+C")
    try:
        tornado.ioloop.IOLoop.current().start()
    except KeyboardInterrupt:
        print("Ukončnuji program")
