
# Knihovna pro komunikaci s elektronikou Xioami
# python-miio.readthedocs.io/
from miio import FanMiot, DeviceException

# Knihovna pro práci s HTTP serverem
# https://www.tornadoweb.org
import asyncio
import tornado
import json

# NASTAVENÍ PŘÍSTUPOVÝCH ÚDAJŮ K VĚTRÁKU
# Tyto informace získáte příkazem: miiocli cloud
ip = "lan-ip-adresa-vetraku"
token = "token-vetraku"
vetrak = None

class MainHandler(tornado.web.RequestHandler):
    def get(self):
        global vetrak
        povel = self.get_argument("povel", "")

        if povel == "data":
            try:
                status = vetrak.status().data
                status["chyba"] = False
                self.write(json.dumps(status))
            except DeviceException as e:
                status = {
                    "chyba": True,
                    "informace": str(e)
                }
                self.write(json.dumps(status))

        elif povel == "zmena":
            id = self.get_argument("id", "")
            hodnota = self.get_argument("hodnota", "")
            try:
                vetrak.set_property(id, int(hodnota))
                status = {
                    "chyba": False,
                    "parametr": {
                        "id": id,
                        "hodnota": hodnota
                    }
                }
                self.write(json.dumps(status))
            except DeviceException as e:
                status = {
                    "chyba": True,
                    "informace": str(e)
                }
                self.write(json.dumps(status))
        
        else:
            self.render("vetrak.html")

async def main():
    global vetrak
    global ip
    global token
    vetrak = FanMiot(ip, token)
    web = tornado.web.Application([
        (r"/", MainHandler),
    ])
    web.listen(80)
    await asyncio.Event().wait()
    

if __name__ == "__main__":
    asyncio.run(main())
