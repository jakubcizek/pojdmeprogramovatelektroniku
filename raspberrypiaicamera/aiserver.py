import socket
print("Buďte trpěliví, inicializace aplikace může trvat až několik desítek sekund!", flush=True)
print(f"Server bude po spuštění dostupný na: http://{socket.gethostname()}.local:8000")

import io
import threading
import time
import json
import os

import tornado.web
import tornado.websocket
import tornado.locks
from tornado.ioloop import IOLoop, PeriodicCallback

from picamera2 import Picamera2
from picamera2.devices import IMX500
from picamera2.encoders import JpegEncoder
from picamera2.outputs import FileOutput

# --- KONFIGURACE ---
WIDTH, HEIGHT = 640, 480
MODEL_PATH = "/usr/share/imx500-models/imx500_network_ssd_mobilenetv2_fpnlite_320x320_pp.rpk"
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# --- NAČTENÍ LABELŮ ---
LABELS = []
try:
    with open(os.path.join(BASE_DIR, "labels.txt"), "r") as f:
        LABELS = [l.strip() for l in f.read().split("\n") if l.strip()]
except FileNotFoundError:
    LABELS = ["Objekt"] * 100

# --- GLOBÁLNÍ STAV ---
output_buffer = io.BytesIO()
buffer_lock = threading.Lock()
telemetry_lock = threading.Lock()
async_condition = tornado.locks.Condition()
main_loop = None

# Řízení běhu aplikace
is_active = False
latest_telemetry_data = [] 
detection_threshold = 0.50


class TelemetryHandler(tornado.websocket.WebSocketHandler):
    """
    Obsluhuje WebSocket spojení pro posílání telemetrických dat (detekce) klientům.
    Udržuje seznam aktivních klientů a umožňuje hromadné rozesílání zpráv.
    """
    clients = set()

    def open(self):
        TelemetryHandler.clients.add(self)

    def on_close(self): 
        if self in TelemetryHandler.clients:
            TelemetryHandler.clients.remove(self)

    @classmethod
    def broadcast(cls, message):
        """Odešle zprávu všem připojeným klientům."""
        for client in cls.clients:
            try:
                client.write_message(message)
            except:
                pass


class StreamingOutput(io.BufferedIOBase):
    """
    Buffer pro zachytávání MJPEG snímků z kamery.
    Zajišťuje vláknově bezpečný zápis a notifikuje čekající klienty o novém snímku.
    """
    def __init__(self):
        self.last_write = 0
        self.min_interval = 1.0 / 30.0  # Omezovač FPS (cca 30 FPS)

    def write(self, buf):
        if not is_active:
            return
        
        now = time.time()
        if now - self.last_write < self.min_interval:
            return
        self.last_write = now
        
        with buffer_lock:
            output_buffer.truncate(0)
            output_buffer.seek(0)
            output_buffer.write(buf)
        
        if main_loop:
            main_loop.add_callback_from_signal(self.notify)

    def notify(self):
        async_condition.notify_all()


class StreamHandler(tornado.web.RequestHandler):
    """
    Poskytuje MJPEG stream klientovi.
    Využívá 'multipart/x-mixed-replace' pro kontinuální posílání snímků.
    """
    def set_default_headers(self):
        self.set_header('Content-Type', 'multipart/x-mixed-replace; boundary=FRAME')
        self.set_header('Cache-Control', 'no-store, no-cache, must-revalidate, max-age=0')
        
    async def get(self):
        if not is_active:
            self.write(b'')
            return

        while is_active:
            try:
                await async_condition.wait()
                if not is_active:
                    break
                
                with buffer_lock:
                    frame = output_buffer.getvalue()
                
                self.write(b'--FRAME\r\nContent-Type: image/jpeg\r\nContent-Length: ' + str(len(frame)).encode() + b'\r\n\r\n')
                self.write(frame)
                self.write(b'\r\n')
                await self.flush()
            except:
                break


class ControlHandler(tornado.web.RequestHandler):
    """
    REST API endpoint pro ovládání aplikace.
    Přijímá příkazy start/stop a nastavení prahu detekce.
    """
    def post(self):
        global detection_threshold, is_active
        try:
            data = json.loads(self.request.body)
            
            if "command" in data:
                cmd = data["command"]
                if cmd == "start":
                    is_active = True
                    print("Příkaz: START")
                elif cmd == "stop":
                    is_active = False
                    print("Příkaz: STOP")
            
            if "threshold" in data:
                detection_threshold = float(data["threshold"])
                print(f"Práh: {detection_threshold}")
                
            self.write({"status": "ok", "active": is_active})
        except:
            self.write({"status": "error"})


class LabelsHandler(tornado.web.RequestHandler):
    """
    Vrací seznam všech známých tříd (objektů) jako JSON.
    Slouží pro našeptávač v klientské aplikaci.
    """
    def get(self):
        self.set_header("Content-Type", "application/json")
        self.write(json.dumps(LABELS))


class IndexHandler(tornado.web.RequestHandler):
    """
    Obsluhuje hlavní stránku aplikace.
    Nastavuje hlavičky pro zákaz cachování, aby byl vývoj UI pohodlnější.
    """
    def set_default_headers(self):
        self.set_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
        self.set_header("Pragma", "no-cache")
        self.set_header("Expires", "0")

    def get(self):
        self.render("index.html")


def parse_detections(metadata, imx500_dev):
    """
    Zpracuje metadata z kamery, extrahuje detekce a aktualizuje globální stav.
    Provádí normalizaci souřadnic a filtraci podle prahu spolehlivosti.
    """
    global latest_telemetry_data
    
    if not is_active:
        with telemetry_lock:
            latest_telemetry_data = []
        return

    output = imx500_dev.get_outputs(metadata)
    current_data = []

    if output is not None:
        try:
            # Sjednocení formátu výstupu (dictionary vs list)
            if isinstance(output, dict):
                r_boxes = output.get("output_0", output.get(0))
                r_scores = output.get("output_1", output.get(1))
                r_classes = output.get("output_2", output.get(2))
            else:
                r_boxes = output[0]
                r_scores = output[1]
                r_classes = output[2]
            
            # Ošetření dimenzí polí
            boxes = r_boxes[0] if r_boxes.ndim == 3 else r_boxes
            scores = r_scores[0] if r_scores.ndim == 2 else r_scores
            classes = r_classes[0] if r_classes.ndim == 2 else r_classes

            for i in range(len(scores)):
                score = float(scores[i])
                
                if score > detection_threshold:
                    cls_id = int(classes[i])
                    lbl = LABELS[cls_id] if 0 <= cls_id < len(LABELS) else f"ID_{cls_id}"
                    
                    # Přepočet normalizovaných souřadnic na pixely
                    raw_box = boxes[i]
                    y0, x0, y1, x1 = float(raw_box[0]), float(raw_box[1]), float(raw_box[2]), float(raw_box[3])
                    
                    x = int(x0 * WIDTH)
                    y = int(y0 * HEIGHT)
                    w = int((x1 - x0) * WIDTH)
                    h = int((y1 - y0) * HEIGHT)
                    
                    current_data.append({
                        "label": lbl, 
                        "score": score, 
                        "box": [max(0, x), max(0, y), w, h]
                    })
        except Exception: 
            pass

    with telemetry_lock:
        latest_telemetry_data = current_data


def camera_thread_func():
    """
    Hlavní smyčka kamery.
    Inicializuje hardware (IMX500, Picamera2), nahrává model a spouští capture loop.
    Reaguje na globální proměnnou 'is_active' pro pozastavení zpracování.
    """
    try:
        hostname = socket.gethostname()
        local_url = f"http://{hostname}.local:8000"

        # 1. Inicializace IMX500 (Nahrání modelu do FW)
        if os.path.exists(MODEL_PATH):
            imx500 = IMX500(MODEL_PATH)
        else:
            print(f"CHYBA: Model {MODEL_PATH} neexistuje!")
            os._exit(1)

        # 2. Inicializace kamery
        picam2 = Picamera2(imx500.camera_num)
        picam2.configure(picam2.create_video_configuration(main={"size": (WIDTH, HEIGHT), "format": "RGB888"}))
        picam2.start()
        
        output = StreamingOutput()
        picam2.start_recording(JpegEncoder(), FileOutput(output))
        
        while True:
            if not is_active:
                time.sleep(0.1)
                continue

            metadata = picam2.capture_metadata()
            parse_detections(metadata, imx500)
            time.sleep(0.005)
            
    except Exception as e:
        print(f"KAMERA ERROR: {e}")
        os._exit(1)


def ws_update_loop():
    """
    Pravidelná úloha (PeriodicCallback), která odesílá aktuální detekce klientům.
    Pokud nejsou žádná data, nic neodesílá, aby se předešlo problikávání grafu.
    """
    with telemetry_lock:
        data_to_send = list(latest_telemetry_data)
    
    if not data_to_send:
        return

    try:
        if TelemetryHandler.clients:
            msg = json.dumps(data_to_send)
            TelemetryHandler.broadcast(msg)
    except:
        pass


def make_app():
    """
    Vytváří a konfiguruje instanci Tornado aplikace.
    Mapuje URL na handlery a vypíná cache šablon pro vývoj.
    """
    return tornado.web.Application([
        (r'/', IndexHandler),
        (r'/stream.mjpg', StreamHandler),
        (r'/control', ControlHandler),
        (r'/labels', LabelsHandler),
        (r'/ws', TelemetryHandler),
    ], 
    template_path=os.path.join(BASE_DIR, "./"),
    compiled_template_cache=False,
    debug=True
    )


if __name__ == '__main__':
    main_loop = IOLoop.current()
    
    # Spuštění kamery v samostatném vlákně
    threading.Thread(target=camera_thread_func, daemon=True).start()
    
    app = make_app()
    app.listen(8000)
    
    # Spuštění pravidelného odesílání WS dat (každých 100ms)
    PeriodicCallback(ws_update_loop, 100).start()
    
    try:
        main_loop.start()
    except KeyboardInterrupt:
        print("Ukončuji...")