## Vyhledávač nejbližšího deště 
Příloha článku v časopisu **Computer 11/2025**. Více zde: [ikiosek.cz/computer](https://www.ikiosek.cz/computer).

- **prsi.py**: Základní ukázka kódu, kestl iprší na určité XY pozici. Tento kód najdete včetně komentáře přímo v článku v časopisu Computer
- **server.py**: Kompletní server a JSON API napsaný v Python us využitím knihovny Tornado. Identický server běží na testovací adrese **[https://radar.kloboukuv.cloud](https://radar.kloboukuv.cloud)**

Testovací server běží na slabém virtuálním počítači v infrastruktuře Oracle Cloud. Výpočet nejbližšího deště tak může při větší zátěži zabrat nějaký čas.

## Aktualizace: 27. dubna 2026 ##

Zatímco v článku v časopisu Computer demonstruji vyhledávání nejbližších barevných pixelů algoritmem BFS (prohledávání do šířky), nová verze v `server.py` používá **vektorové operace nad NumPy maticemi**. Výsledkem je řádové zrychlení a zároveň oprava dvou skrytých vad původního BFS.

### V čem byla BFS pomalá

BFS prochází pixely v Python smyčce a pro každý jeden navštívený pixel volá:

- přístup do PIL bitmapy (`pixels[x, y]` má kvůli režii Pythonu náklady ve stovkách nanosekund až mikrosekundách na pixel),
- lineární vyhledání nejbližší barvy ze 15-prvkové tabulky barevné stupnice,
- další Python smyčku přes okolní okno pro kontrolu velikosti deště.

Při `max_distance = 50` to znamenalo navštívit až ~7 800 pixelů a každý zatížit ~100 dílčími operacemi v interpretovaném Pythonu. Výsledkem byly desítky milisekund na dotaz, i když se radarový snímek mezi voláními vůbec nezměnil.

### Vektorová analýza stojí na dvou pilířích

**1. Předvýpočet jednou za snímek.** Radarový snímek se aktualizuje jednou za 5 minut, dotazy chodí podstatně častěji. Funkce `precomputeRadarMatrices()` proto po každém stažení snímku připraví čtyři pomocné NumPy matice (a uloží je do globálních proměnných), aby se nemusely počítat pro každý HTTP dotaz znovu:

| matice | rozměr | význam |
|---|---|---|
| `rain_mask` | H × W bool | `True` tam, kde pixel obsahuje déšť (alpha > 0 a alespoň jeden RGB kanál ≠ 0) |
| `dbz_arr` | H × W int8 | úroveň dBZ pro každý deštivý pixel — určená nejbližší shodou s referenčními barvami stupnice |
| `mmh_arr` | H × W float64 | odpovídající úhrn srážek v mm/h |
| `ii_arr` | (H+1) × (W+1) int32 | **integrální obraz** (summed-area table) masky `rain_mask` — umožňuje v konstantním čase O(1) zjistit, kolik deštivých pixelů obsahuje libovolný obdélník v obraze |

Klíčový krok je odečet barvy: pro každý deštivý pixel se v jednom NumPy výrazu přes broadcasting porovnávají všech 15 referenčních barev naráz, místo 15 sériových porovnání v Pythonu. Celý předvýpočet doběhne v jednotkách milisekund.

**2. Hledání bez Python smyčky.** Funkce `findNearestRain` při každém dotazu:

1. Vyřízne z předpočítaných matic okolí pozorovatele `[y ± max_distance, x ± max_distance]`. Pokud výsek přesahuje hranice obrazu, sám se ořeže — žádné `IndexError`, žádné kontroly v ručních smyčkách.
2. Pomocí broadcastingu (`np.ogrid`) spočítá pro celý výsek naráz Euklidovskou vzdálenost² každého pixelu od pozorovatele.
3. Vektorovou indexací do integrálního obrazu zjistí pro každého kandidáta najednou, kolik deštivých pixelů má v okně okolo sebe.
4. Jedinou booleovskou maskou kombinuje tři podmínky — minimální mm/h, minimální počet pixelů v okně, vzdálenost ≤ poloměru.
5. `np.argmin` nad maskovanou maticí vzdáleností² vrátí pozici nejbližšího pixelu, který všechny podmínky splňuje.

Celý výpočet je tedy několik NumPy výrazů nad poli — žádná Python smyčka přes pixely. To je „vektorové" v praxi: nahradíme `for pixel in image:` jednou maticovou operací, kterou NumPy vykoná v interní C smyčce.

### Naměřené zrychlení

Brno (49.195°N, 16.606°E), snímek ČHMÚ 680 × 460 px:

| `max_distance` | BFS (původní) | vektorová verze |
|---:|---:|---:|
| 20 km | ~9 ms | **0.13 ms** |
| 50 km | ~26 ms | **1 ms** |
| 150 km | (limitováno) | **2.4 ms** |
| celý radarový snímek | (zhroucení) | **~10 ms strop** |

Vektorové vyhledávání je oproti BFS přibližně **20–50× rychlejší** a strop ~10 ms platí i pro hledání přes celý radarový snímek — výseky se nikdy nezpracovávají větší, než kolik vejde do bitmapy.
