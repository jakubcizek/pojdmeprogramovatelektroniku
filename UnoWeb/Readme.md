# UnoWeb — minimalistický WWW server na ATMEGA328P

Demonstrace, že plnohodnotný HTTP server běží i na **ATMEGA328P se2 kB SRAM**.
Zobrazuje teplotu a vlhkost ze senzoru **SHT31** (I2C) přes vestavěný
ethernetový čip **WIZnet W5500**. Vytvořeno v prostředí **Arduino** pro **Platformio IDE**.

## Naměřené využití paměti

Hodnoty pro výchozí `NSOCK=8` (release, bez logování):

| | Použito | Z celku |
|---|---|---|
| SRAM (globální) | **114 B** | 2048 B (5,6 %) |
| Flash | **7 472 B** | 31,5 kB (23,2 %) |

`NSOCK=1` → 79 B SRAM, každý další socket +5 B (viz
tabulka škálování níže). Aplikační stav: teplota+vlhkost (4 B), počítadla
dotazů (8 B), rx/tx bajty (8 B), uptime (3 B), síťová konfigurace (~12 B),
per-socket stav (5 B × NSOCK), drobné proměnné. HTML stránka je **celá ve
flash** (PROGMEM) a streamuje se přímo do TX bufferu W5500 — žádný velký
RAM buffer, RAM **neroste se zátěží**.

## Hardware a pinout

Keyestudio W5500 Ethernet Development Board (Arduino UNO + W5500):

| Signál | Pin | Port |
|---|---|---|
| W5500 SCS | D10 | PB2 |
| W5500 MOSI / MISO / SCK | D11 / D12 / D13 | PB3 / PB4 / PB5 |
| SHT31 SDA / SCL | A4 / A5 | TWI |

**W5500 RST:** používá se **softwarový reset** přes registr MR (žádný GPIO).
Pokud má deska RST vyvedený, odkomentuj `#define W5500_RST_PIN` v
[config.h](src/config.h).

## Architektura

- **Boot:** init SPI → SW reset W5500 → **DHCP** (DISCOVER/REQUEST/ACK) →
  při selhání fallback statická IP → `NSOCK` socketů do TCP LISTEN na portu 80.
- **Smyčka:** každých 60 s jedno měření SHT31 (uloží 4 B do SRAM); pak se
  round-robin obslouží každý z `NSOCK` socketů — request se přečte a zahodí
  (neparsuje se), na `GET /` se odešle stránka jedním `SEND`, na ostatní
  cesty minimální `404`, spojení se zavře.

### Škálovatelnost socketů (`NSOCK` v [config.h](src/config.h))

W5500 má 8 nezávislých HW socketů. `NSOCK` (1–8) volí kompromis robustnost
vs. RAM — stav stojí **+5 B SRAM na socket** (`uint32_t connStart` +
`uint8_t prevSr`); `NSOCK=1` = původní spotřeba. Naměřeno (release):

| NSOCK | SRAM | 1000 spojení / 8 vláken | Selhání | Propustnost | p95 |
|---|---|---|---|---|---|
| 1 | 79 B | 50,6 s | 17 | 19 req/s | 1530 ms |
| 4 | 94 B | 9,8 s | 1 | 102 req/s | 532 ms |
| **8** (vých.) | **114 B** | **9,3 s** | **0** | **108 req/s** | **70 ms** |

Propustnost nad `NSOCK=4` už neroste (strop čipu ~105–110 req/s, sekvenční
`loop()` + SPI); `NSOCK=8` přidává **robustnost a konzistenci** (0 odmítnutí
při 8souběžných, p95 70 ms místo 532 ms) — proto výchozí pro veřejné vystavení.

Surová odezva čipu je ~9–30 ms; více socketů jen odstraní frontu/odmítnutí
(HW backlog), proto je `NSOCK=4` doporučeno pro veřejné vystavení.
- **Stránka:** `HTTP/1.0` + `Connection: close` (tělo končí uzavřením, není
  nutné počítat `Content-Length`), `<meta http-equiv=refresh content=60>`.

## Optimalizace

- Raw SPI přes `SPDR` (fck/2 = 8 MHz, max AVR), CS přes `PORTB` místo
  `digitalWrite`, W5500 VDM rámec, celá odpověď jedním burstem.
- SHT31 přes přímé TWI registry, žádné `Wire`/`String`/Ethernet knihovny.
- `-Os -flto`, `--gc-sections`.

## Statistika přístupů

Na stránce se zobrazuje od startu čipu: počet dotazů na stránku (`GET /`)
a celkový počet HTTP `GET` dotazů (rozlišeno, protože prohlížeče dělají
i další požadavky, např. na `favicon.ico`), dále přijaté bajty (↓) a
odeslané bajty (↑) na HTTP socketu. DHCP režie se nepočítá.

## Sestavení a nahrání

```sh
pio run            # build
pio run -t upload  # nahrát do desky
pio device monitor # volitelně, jen s #define APP_DEBUG v config.h
```

## Benchmark

Čísla v tabulce škálování výše byla naměřena skriptem
[benchmark.py](benchmark.py) — zátěžový / brute-force HTTP test bez
externích závislostí (pouze standardní knihovna Pythonu 3).

Otevře `-n` TCP spojení (volitelně v `-c` vláknech), na každém pošle
`GET / HTTP/1.0` a měří dobu celé transakce (connect + send + příjem
těla až po uzavření spojení serverem). Na závěr vypíše propustnost
(req/s), počet úspěchů/selhání, přenesené bajty a statistiku doby
odezvy (min, průměr, medián, p95, p99, max, směrodatná odchylka).
Selhání se počítají zvlášť, aby nezkreslila statistiku úspěšných
transakcí.

```sh
python benchmark.py [host] [-n POČET] [-c VLÁKEN] [--timeout S] [--port P]

# příklad: 1000 spojení v 8 vláknech proti desce na 192.168.1.52
python benchmark.py 192.168.1.52 -n 1000 -c 8
```

| Argument | Výchozí | Význam |
|---|---|---|
| `host` | `192.168.1.52` | IP/hostname desky (poziční) |
| `-n`, `--requests` | `1000` | celkový počet spojení |
| `-c`, `--concurrency` | `8` | počet souběžných vláken |
| `--timeout` | `5` | timeout na spojení v sekundách |
| `--port` | `80` | cílový TCP port |

## Konfigurace

Vše v [src/config.h](src/config.h): MAC adresa, fallback IP/brána/maska,
HTTP port, interval měření, volitelný `APP_DEBUG` a `W5500_RST_PIN`.
