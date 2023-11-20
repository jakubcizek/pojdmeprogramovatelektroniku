## Ukázka lokalizace tanku s flow kamerou PixArt Imaging PAA5100JE
Tentokrát budeme pokračovat v našich hrátkách s optical flow senzory, které najdete i ve své myši. Vyzkoušíme si totiž práci s čipem PAA5100JE. Přimontujeme ho na tank a při jízdě se bude v plátně Canvas v jeho webové ovládací aplikaci kreslit trajektorie a směr pohybu, 

Náš firmware běží v Arduinu na čipu Espressif ESP32-S3 a na prototypovací desce **[ESP32-S3-DevKitC-1](docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/esp32s3/user-guide-devkitc-1.html)**. Tanku pomáhá ještě jendoduchý gyroskop MPU-6040, flow kamera smaotná totiž nedokáže detekovat otočení o devadesát stupňů. Gyroksop bychom mohli nahradit také otáčkoměrem, nebo ještě lépe robotem s všesměrovými koly (mecanum), který bude držet stále stejnou orientaci v prostoru.
 - **[Článek a video na Živě.cz](https://www.zive.cz/clanky/flow-kamera-z-mysi-muze-fungovat-jako-gps-primontovali-jsme-ji-na-tank-a-jezdime-po-redakci/sc-3-a-225289/default.aspx)**
 - **[Produktová stránka flow kamery PAA5100JE](www.pixart.com/products-detail/74/PAA5100JE-Q)**
 - **[Datasheet flow kamery PAA5100JE](cdn.shopify.com/s/files/1/0174/1800/files/PAA5100JE-Q-GDS-R1.00_25072018.pdf?v=1620146590)**
 - **[Prototypovací modul Pimoroni PAA5100JE](https://shop.pimoroni.com/products/paa5100je-optical-tracking-spi-breakout?variant=39315330170963)**
 - **[Knihovna pro Arudino](https://github.com/jakubcizek/Optical_Flow_Sensors_Arduino)**
