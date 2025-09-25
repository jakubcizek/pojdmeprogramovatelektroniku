# Čtvrtý program na ESP32-P4

**Příloha článku „Stíhačka z Asie ESP32-P4“**, kterému se věnuji v časopisu [Computer 10/2025](https://www.ikiosek.cz/computer).

Program spustí dva jednoduché HTTP servery. Na portu 80 běží webový ovládací server a na portu 81 streamovací MJPEG server. Základní webový server zobrazí ovládací HTML stránku s možností pořízení JPEG snímku a spuštění MJPEG streamu. **Oproti programu číslo 3 se tento liší v tom, že namísto Wi-Fi používá mnohem rychlejší ethernet.** MJPEG video je při rozlišení 800×640px plynulé, protože nás nebrzdí složitá Wi-Fi komunikace skrze ESP Hosted/Remote Wi-Fi na sekundárním čipu ESP32-C6. 

- Použitý hardware: [Waveshare ESP32-P4-ETH](https://www.waveshare.com/esp32-p4-eth.htm) (zmenšenína oficiálního devkitu od Espressifu, varianta s ethernetem a RJ-45 namísto Wi-Fi)
- Použitá kamera: [Libovolný klon RPi kamery se snímačem OV5647 a rozhraním MIPI CSI](https://www.waveshare.com/rpi-camera-b.htm)
- Propojovací kabel kamery s užší/Mini koncovkou pro konektor desky: [Raspberry Pi Camera Cable Standard - Mini 200 mm](https://rpishop.cz/mipi/6501-raspberry-pi-5-camera-cable-standard-mini-200-mm.html)
- Vývojové prostředí: [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/get-started/index.html)
