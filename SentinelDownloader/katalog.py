from oauthlib.oauth2 import BackendApplicationClient
from requests_oauthlib import OAuth2Session

# Prihlasovaci udaje pro OAuth
# Vytvor zde: shapps.dataspace.copernicus.eu/dashboard/#/account/settings
client_id = ""
client_secret = ""

if len(client_id) == 0 or len(client_secret) == 0:
    print("Vypln client_id a client_secret pro OAuth autentizaci!")
    print("Zaloz si bezplatny ucet na Copernicus a bez sem: https://shapps.dataspace.copernicus.eu/dashboard/#/account/settings")
    exit()

# Parametry API dotazu
parametry_dotazu = {
    "bbox": [13.960876,49.841525,14.988098,50.296358],
    "datetime": "2024-09-01T00:00:00Z/2024-09-07T23:59:59Z",
    "collections": ["sentinel-2-l2a"],
    "limit": 5
}

# OAuth autentizace
client = BackendApplicationClient(client_id=client_id)
oauth = OAuth2Session(client=client)
token = oauth.fetch_token(token_url="https://identity.dataspace.copernicus.eu/auth/realms/CDSE/protocol/openid-connect/token", client_secret=client_secret, include_client_id=True)

# Dotaz na Catalog API
odpoved = oauth.post("https://sh.dataspace.copernicus.eu/api/v1/catalog/1.0.0/search", json = parametry_dotazu)
if odpoved.status_code == 200:
    data = odpoved.json()
    if "features" in data:
        print(f"Nalezeno zaznamu: {len(data['features'])}:")
        for i,feature in enumerate(data["features"]):
            print(f"{i}. zaznam:")
            print(f"\tDatum: {feature['properties']['datetime']}")
            print(f"\tPlatforma: {feature['properties']['platform']} (Zarizeni: {','.join(feature['properties']['instruments'])})")
            print(f"\tZakryti mraky: {feature['properties']['eo:cloud_cover']} %")
            print("")
else:
    print(f"Chybicka se vloudila!\nHTTP kod: {odpoved.status_code}\n{odpoved.text}")
