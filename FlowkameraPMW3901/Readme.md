## Ukázka spojení s flow kamerou PixArt Imaging PMW3901 
Tentokrát se podíváme na zoubek optical flow senzorům, které najdete i ve své myši. Vyzkoušíme si totiž práci s čipem PMW3901. Sesterský modul od stejného výrobce najdete třeba v počítačových myších od Logitechu (MX Master aj.). V příkladu **Framebuffer** přečteme ze snímače maličkou fotografii v rozlišení 35x35 pixelů. V příkladu **Dron** proměníme kameru v polohovací zařízení a pohyb kamery v osách XY budeme přenášet na obrázek dronu na herním plátně.

Každý příklad se skládá z části pro Arduino, která poběží na desce **[XIAO ESP32C3](https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html)** (nebo jakékoliv jiné s 3,3V logikou) a z části pro **[Processing](https://processing.org/)**, který se na PC postará o samotné zobrazení výsledku v okně. Komunikace bude probíhat po sériové lince.
 - **[Článek a video na Živě.cz](https://www.zive.cz/clanky/programujeme-flow-kameru-stejnou-najdete-ve-sve-pocitacove-mysi-a-na-dronech/sc-3-a-224570/default.aspx)**
 - **[Produktová stránka flow kamery PMW3901](https://www.pixart.com/products-detail/44/PMW3901MB-TXQT)**
 - **[Datasheet flow kamery PMW3901](https://octopart.com/datasheet/pmw3901mb-txqt-pixart-77804687)**
 - **[Knihovna pro Arudino od Bitcraze](https://github.com/bitcraze/Bitcraze_PMW3901)**
