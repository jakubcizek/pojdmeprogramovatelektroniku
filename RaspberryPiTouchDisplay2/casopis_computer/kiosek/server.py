# https://www.tornadoweb.org/en/stable/
import tornado.ioloop
import tornado.web
import os
import subprocess

# Trida pro obsluhu HTTP dotazu na cestu /
class MainHandler(tornado.web.RequestHandler):
    def get(self):
        cmd = self.get_argument("cmd", None)
        if cmd == "lcd-off":
            # Vypnutí displeje
            subprocess.run(["sudo", "tee", "/sys/class/backlight/11-0045/bl_power"], input="1", text=True)
            self.write("LCD vypnuto")
        elif cmd == "lcd-on":
            # Zapnutí displeje
            subprocess.run(["sudo", "tee", "/sys/class/backlight/11-0045/bl_power"], input="0", text=True)
            self.write("LCD zapnuto")
        else:
            with open("kiosek.html", "r") as file:
                self.write(file.read())

# Funkce pro konfiguraci serveru
# Na dotazy /icons/* bude staticky odpovidat soubory v totoznem podadresari
# Na dotazy / bude dynamicky odpovidat v tride MainHandler  
def get_server():
    icons_dir = os.path.join(os.path.dirname(__file__), "icons")
    return tornado.web.Application([
        (r"/", MainHandler),
        (r"/icons/(.*)", tornado.web.StaticFileHandler, {"path": icons_dir}),
    ])

# Zacatek programu
# Server bezi na standardnim TCP portu pro HTTP 80
# Pro spusteni tedy budeme potrebovat prava spravce/sudo
if __name__ == "__main__":
    port = 80
    server = get_server()
    server.listen(port)
    print(f"Server běží na adrese http://localhost:{port}")
    tornado.ioloop.IOLoop.current().start()
