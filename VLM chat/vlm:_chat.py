#!/usr/bin/env python3
"""VLM chat web server - Tornado + WebSocket + MJPEG stream + Hailo-10H + RPi kamera."""

import os
import sys
import json
import signal
import socket
import threading
import time
import multiprocessing as mp

os.environ["LIBCAMERA_LOG_LEVELS"] = "ERROR"

import cv2
import numpy as np
import tornado.ioloop
import tornado.web
import tornado.websocket
from picamera2 import Picamera2
from hailo_platform import VDevice
from hailo_platform.genai import VLM

PORT = 8080 # TCP port HTTP serveru

# Cesta k modelu
# Pokud model zatim nemame, skript se ho pokusi stahnout
# POZOR, MODEL MA VELIKOST 2.2 GB
HEF_PATH = "./Qwen2-VL-2B-Instruct.hef"

# MAX_TOKENS - maximalni pocet generovanych tokenu v jedne odpovedi.
#   Nizsi hodnota = kratsi a rychlejsi odpoved, vyssi = podrobnejsi ale pomalejsi.
#   Rozsah: 1 az ~2048 (zalezi na modelu). Doporuceno: 100-500.
MAX_TOKENS = 200

# TEMPERATURE - mira nahodnosti pri generovani textu.
#   0.0 = deterministicky (vzdy stejna odpoved), 1.0 = hodne kreativni/nahodny.
#   Pro presny popis obrazu pouzijte nizke hodnoty (0.01-0.3),
#   pro kreativnejsi odpovedi zvyste (0.5-0.9).
TEMPERATURE = 0.1

# SEED - pocatecni hodnota generatoru nahodnych cisel.
#   Stejna hodnota = reprodukovatelne vysledky pri stejnem vstupu.
#   Zmente na jine cislo nebo None pro jiny/nahodny vystup.
SEED = 42
SYSTEM_PROMPT = "You are a helpful assistant that analyzes images and answers questions about them."  # Systemovy prompt pro VLM
MJPEG_FPS = 15  # Snimkova frekvence MJPEG streamu
MJPEG_QUALITY = 70  # Kvalita JPEG komprese (0-100)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))  # Adresar se skriptem
HTML_PAGE = open(os.path.join(BASE_DIR, "index.html"), encoding="utf-8").read()  # HTML obsah hlavni stranky

picam2 = None  # Instance Picamera2
request_q = None  # Fronta pozadavku pro VLM worker
response_q = None  # Fronta odpovedi z VLM workeru
worker = None  # VLM worker proces
latest_jpeg = None  # Posledni JPEG frame pro MJPEG stream
jpeg_lock = threading.Lock()


def download_hef_model():
    """Stahne HEF model z Hailo serveru do ciloveho adresare."""
    import urllib.request
    import tempfile

    # URL se sklada z verze HailoRT a nazvu modelu
    hailort_version = "5.1.1"
    try:
        import subprocess
        out = subprocess.check_output(["hailortcli", "--version"], text=True).strip()
        # Format: "HailoRT-CLI version X.Y.Z"
        hailort_version = out.split()[-1]
    except Exception:
        pass

    model_name = os.path.basename(HEF_PATH)
    url = f"https://dev-public.hailo.ai/v{hailort_version}/blob/{model_name}"

    dest_dir = os.path.dirname(HEF_PATH)
    os.makedirs(dest_dir, exist_ok=True)

    print(f"Stahuji model (~2.3 GB):\n  {url}")

    # Stazeni do docasneho souboru, pak presun na cilovou cestu (atomicka operace)
    fd, tmp_path = tempfile.mkstemp(dir=dest_dir, prefix=f".{model_name}.", suffix=".tmp")
    os.close(fd)

    try:
        req = urllib.request.Request(url, headers={
            "User-Agent": "Mozilla/5.0 (Linux; aarch64) vlm_server/1.0",
        })
        with urllib.request.urlopen(req, timeout=600) as resp:
            total = int(resp.headers.get("Content-Length", 0))
            downloaded = 0
            with open(tmp_path, "wb") as f:
                while True:
                    chunk = resp.read(256 * 1024)
                    if not chunk:
                        break
                    f.write(chunk)
                    downloaded += len(chunk)
                    if total > 0:
                        pct = downloaded * 100 // total
                        mb = downloaded / (1024 * 1024)
                        total_mb = total / (1024 * 1024)
                        print(f"\r  {pct}% ({mb:.0f}/{total_mb:.0f} MB)", end="", flush=True)
            print()

        # Overeni velikosti
        if total > 0 and os.path.getsize(tmp_path) != total:
            raise RuntimeError("Stahovani se nepodarilo dokoncit (nesouhlasi velikost souboru)")

        os.rename(tmp_path, HEF_PATH)
        print(f"Model ulozen: {HEF_PATH}")

    except Exception:
        # Smazat nedokonceny soubor
        if os.path.exists(tmp_path):
            os.unlink(tmp_path)
        raise


def check_hef_model():
    """Overi dostupnost HEF modelu. Pokud chybi, pokusi se ho stahnout."""
    if os.path.isfile(HEF_PATH):
        return

    print(f"HEF model nenalezen: {HEF_PATH}")

    try:
        download_hef_model()
    except Exception as e:
        print(f"Stahovani modelu selhalo: {e}")
        sys.exit(1)


def center_crop_resize(image, target_size=(336, 336)):
    """Orizne obraz na stred a zmeni jeho velikost na cilovy rozmer."""
    h, w = image.shape[:2]
    tw, th = target_size
    scale = max(tw / w, th / h)
    resized = cv2.resize(image, (int(w * scale), int(h * scale)), interpolation=cv2.INTER_LINEAR)
    nh, nw = resized.shape[:2]
    x0 = (nw - tw) // 2
    y0 = (nh - th) // 2
    return resized[y0:y0 + th, x0:x0 + tw].astype(np.uint8)


def camera_capture_loop(stop_event):
    """Vlakno: zachytava snimky z kamery a koduje je do JPEG pro MJPEG stream."""
    global latest_jpeg
    interval = 1.0 / MJPEG_FPS
    while not stop_event.is_set():
        frame = picam2.capture_array()
        _, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, MJPEG_QUALITY])
        with jpeg_lock:
            latest_jpeg = buf.tobytes()
        time.sleep(interval)


def vlm_worker_fn(req_queue, resp_queue):
    """Worker proces - drzi VLM model, posila odpovedi po chunkach."""
    try:
        params = VDevice.create_params()
        params.group_id = "SHARED"
        vdevice = VDevice(params)
        vlm = VLM(vdevice, HEF_PATH)

        while True:
            item = req_queue.get()
            if item is None:
                break

            try:
                prompt = [
                    {"role": "system", "content": [{"type": "text", "text": item["system_prompt"]}]},
                    {"role": "user", "content": [{"type": "image"}, {"type": "text", "text": item["user_prompt"]}]},
                ]

                start = time.time()
                with vlm.generate(prompt=prompt, frames=[item["image"]],
                                  temperature=TEMPERATURE, seed=SEED,
                                  max_generated_tokens=MAX_TOKENS) as generation:
                    for chunk in generation:
                        if chunk != "<|im_end|>":
                            resp_queue.put({"type": "chunk", "text": chunk})

                vlm.clear_context()
                elapsed = time.time() - start
                resp_queue.put({"type": "done", "elapsed": round(elapsed, 1)})
            except Exception as e:
                resp_queue.put({"type": "error", "text": str(e)})
    finally:
        try:
            vlm.release()
            vdevice.release()
        except Exception:
            pass


class IndexHandler(tornado.web.RequestHandler):
    """Handler pro hlavni HTML stranku."""

    def get(self):
        """Odesle HTML stranku s uzivatelskym rozhranim."""
        self.write(HTML_PAGE)


class MjpegHandler(tornado.web.RequestHandler):
    """Handler pro MJPEG video stream z kamery."""

    async def get(self):
        """Posila kontinualni MJPEG stream jako multipart HTTP odpoved."""
        self.set_header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
        self.set_header("Cache-Control", "no-cache, no-store, must-revalidate")
        interval = 1.0 / MJPEG_FPS
        loop = tornado.ioloop.IOLoop.current()
        while True:
            with jpeg_lock:
                jpeg = latest_jpeg
            if jpeg:
                try:
                    self.write(b"--frame\r\nContent-Type: image/jpeg\r\n\r\n")
                    self.write(jpeg)
                    self.write(b"\r\n")
                    await self.flush()
                except Exception:
                    return
            await loop.run_in_executor(None, time.sleep, interval)


class ChatWebSocket(tornado.websocket.WebSocketHandler):
    """WebSocket handler pro chat komunikaci s VLM modelem."""

    def open(self):
        """Zaloguje nove WebSocket pripojeni."""
        print("WebSocket pripojen")

    def on_message(self, message):
        """Zpracuje dotaz od uzivatele, zachyti snimek a posle do VLM workeru."""
        data = json.loads(message)
        question = data.get("question", "").strip()
        if not question:
            question = "Describe the image"

        # Zachytit snimek a pripravit pro VLM (BGR→RGB pro model)
        frame = picam2.capture_array()
        image = center_crop_resize(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))

        # Poslat do VLM workeru
        request_q.put({
            "image": image,
            "system_prompt": SYSTEM_PROMPT,
            "user_prompt": question,
        })

        ioloop = tornado.ioloop.IOLoop.current()
        ioloop.add_callback(self._stream_response)

    async def _stream_response(self):
        """Cte odpovedi z VLM workeru a posila je klientovi po chunkach."""
        loop = tornado.ioloop.IOLoop.current()
        while True:
            msg = await loop.run_in_executor(None, response_q.get)
            try:
                self.write_message(json.dumps(msg))
            except tornado.websocket.WebSocketClosedError:
                return
            if msg["type"] in ("done", "error"):
                return

    def on_close(self):
        """Zaloguje odpojeni WebSocket klienta."""
        print("WebSocket odpojen")


def get_local_ips():
    """Vrati seznam ne-loopback IPv4 adres vsech aktivnich sitovych rozhrani."""
    import subprocess
    ips = []
    try:
        out = subprocess.check_output(["hostname", "-I"], text=True).strip()
        if out:
            ips = out.split()
    except Exception:
        pass
    if not ips:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ips = [s.getsockname()[0]]
            s.close()
        except Exception:
            ips = ["127.0.0.1"]
    return ips


def main():
    """Spusti VLM server: inicializuje kameru, worker proces a Tornado webovy server."""
    global picam2, request_q, response_q, worker

    check_hef_model()

    request_q = mp.Queue(maxsize=2)
    response_q = mp.Queue(maxsize=100)
    worker = mp.Process(target=vlm_worker_fn, args=(request_q, response_q), daemon=True)
    worker.start()

    try:
        picam2 = Picamera2()
        config = picam2.create_video_configuration(
            main={"size": (640, 480), "format": "RGB888"},
        )
        picam2.configure(config)
        picam2.start()
    except RuntimeError:
        print("CHYBA: Kamera je obsazena jinym procesem.\n"
              "Zkontrolujte, ktery proces ji blokuje:\n"
              "  ps aux | grep -E 'libcamera|picamera|vlm_server'")
        sys.exit(1)

    # MJPEG capture vlakno
    stop_event = threading.Event()
    cam_thread = threading.Thread(target=camera_capture_loop, args=(stop_event,), daemon=True)
    cam_thread.start()

    app = tornado.web.Application([
        (r"/", IndexHandler),
        (r"/stream", MjpegHandler),
        (r"/ws", ChatWebSocket),
    ])
    app.listen(PORT)

    print(f"\nVLM server bezi na portu {PORT}:")
    for ip in get_local_ips():
        print(f"  http://{ip}:{PORT}/")
    print()

    shutting_down = False

    def shutdown(*_):
        """Signal handler pro graceful ukonceni serveru (SIGINT/SIGTERM)."""
        nonlocal shutting_down
        if shutting_down:
            # Druhy Ctrl+C = okamzite ukonceni
            print("\nVynucene ukonceni.")
            os._exit(1)
        shutting_down = True
        print("\nUkoncuji server...")
        tornado.ioloop.IOLoop.current().add_callback_from_signal(tornado.ioloop.IOLoop.current().stop)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    try:
        tornado.ioloop.IOLoop.current().start()
    finally:
        stop_event.set()
        picam2.stop()
        picam2.close()
        try:
            request_q.put_nowait(None)
        except Exception:
            pass
        worker.join(timeout=2)
        if worker.is_alive():
            worker.terminate()
        print("Server ukoncen.")


if __name__ == "__main__":
    main()
