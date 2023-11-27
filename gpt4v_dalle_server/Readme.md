## Napojení OpenAI GPT-4 Vision a DALL-E 3 na webkmaeru 
Tentokrát napojíme modely GPT-4 Vision a DALL-E 3 od OpenAI na webkameru. Pomůže nám oficiální API. Po spuštění drobného lokálního serveru v Pythonu (python server.py) stačí v prohlížeči a na stejném počítači navštívit adresu **http://localhost**. Zobrazí se stránka s náhledem z připojené webkamery. Pozor, při pokusu o navštívění serveru z jiného počítače (jiné LAN IP) většina prohlížečů nenačte kameru, protože v takovém případě je pro práci s kamerou vyžadovnáno šifrované spojení HTTPS.

Po klepnutí na obraz kamery se uloží obrázek, který se pošle do serveru skrze WebSocket. Server poté obrázek odešle ke zpracování nejprve do GPT-4 Vision, od kterého dostane textový popis scény. Tento popis použije jako prompt pro obrázkový generátor DALL-E 3. Vygenerovaný obrázek opět skrze WebSocket pošle klientovi. WebSocket používáme proto, že umožňuje obousměrnou komunikaci server-klient bez blokování. Celá operace totiž zabrere dlouhé sekundy.

Původní i vygenerovaný obrázek se ukládá do složky images na straně serveru.

V kódu serveru **server.py** je třeba před spuštěním nastavit **vlastní klíč API** pro spojení s OpenAI pod svým účtem. Klíč je uložený v proměnné **api_key**. Také je třeba doinstalovat knuihovny por python **Tornado** a **Requests**. Webový frontend na podporovaných systémech spiouští **vestavěný český hlasový syntetizátor**. Testováno na Windows 11 a v prohlížečích Chrome/Edge, Firefox a Opera, kde se použije systémový český hlas Microsoft Jakub. 

Při spouštění skriptu na WIndows se ujistěte, že **Python běží v režimu UTF-8**. Ten si lze na Windows 11 plošně vynutit v Ovládací panely/Hodiny a oblast/Oblast/Správa/Zmněit místní nastavení systému/Beta: Používat Unicode UTF-8 pro celosvětovou podporu jazyka. Další možností viz Google.

 - **[Článek na Živě.cz](https://www.zive.cz/clanky/napojili-jsme-gpt-4-vision-a-dall-e-3-na-kameru-ai-se-pokousi-kreslit-co-si-mysli-ze-vidi/sc-3-a-225429/default.aspx)**
 - **[Dokumentace GPT-4 Vision API](https://platform.openai.com/docs/guides/vision)**
 - **[Dokumentace DALL-E API](https://platform.openai.com/docs/guides/images?context=node)**
 - **[Tvprba API klíče na OpenAI Platform](https://platform.openai.com/api-keys)**
