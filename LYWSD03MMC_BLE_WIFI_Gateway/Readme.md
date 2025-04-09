## BLE/WiFi brána pro Xiaomi Mi Thermometer (LYWSD03MMC)

Zdrojové kódy pro **článek v časopisu Computer (vydání květen 2025)**, ve kterém si ukážeme, jak na čipu ESP32 (plnohodnotná varianta s Wi-Fi i Bluetooth) spustit jednoduchou bránu, která bude skenovat síť, zachytávat zprávy z teploměrů LYWSD03MMC s komunitním firmwarem (ATC) a posílat data jako JSON na předdefinovaný webový server. V našem demu je to Raspberry Pi v lokální síti.

- **/arduino** – Zdrojový kód pro čip ESP32 a Arduino (včetně volitelného konfiguračního skriptu pro PlatformIO, knihovny [ArduinoJson](https://arduinojson.org/) a [TimeLib](https://github.com/PaulStoffregen/Time))  
- **/python** – Zdrojový kód serveru v Pythonu pro Raspberry Pi (knihovny [Tornado](https://www.tornadoweb.org) a [Requests](https://requests.readthedocs.io))
