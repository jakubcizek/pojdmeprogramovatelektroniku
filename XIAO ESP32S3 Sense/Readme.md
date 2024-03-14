## Minikamera XIAO ESP32S3 Sense

### Přílohy pro článek v časopisu Computer 4/24
 -  **Arduino/XIAOESP32S3SENSE_uart_fotak**: Program, který bude poslouchat na USB/UART. Jakmile dorazí zalomený řádek "FOTO", kamera pořídí 2MP fotografii a uloží ji na microSD jako číslovaný soubor OBRAZEK_0001.JPG
 -  **Arduino/XIAOESP32S3SENSE_timelapse**: Program, který po spuštění vytvoří v microSD číslovaný adresář TL001/ (čítač v persistentní flash) a každé 2 sekundy do něj bude ukládat 2MP fotografie jako číslovaný soubor IMG0001.JPG
 -  **Python/timelapse.py**: Program, který pomocí OpenCV zpracuje snímky z předchozího programu a vytvoří z nich video MP4, do kterého vypálí tři řádky libovolného textu (třeba GPS údaje atp.)

### Příkaz pro složení timelapse fotografií do videa časosběr.mp4 pomocí nástroje [FFMPEG](https://www.ffmpeg.org/download.html):

(příkaz níže předpokládá, že vstupem je adresář TL018, ve kterém jsou obrázky ve formátu IMG0001.JPG - IMG9999.JPG a s poměrem stran 1600x1200px. Výsledné video změnšíme na 1200x900px)

`ffmpeg -framerate 10 -i TL018/IMG%04d.JPG -vf "scale=1200x900" časosběr.mp4 `
