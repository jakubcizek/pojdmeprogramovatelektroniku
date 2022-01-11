import json
import sys
from time import perf_counter

'''
Slovniky pro dilci statistiky zranitlenosti

a ... Aplikace
o ... OS
h ... Hardware/firmware

:family ... Produkt bez verze - rodina

:family:high ... Produkt bez verze u ktereho byla zjisten nejzavaznejsi typ zranitelnosti

'''
zranitelny_produkt = {
        "a": {},
        "o": {},
        "h": {},
        "a:family": {},
        "o:family": {},
        "h:family": {},
        "a:family:high": {},
        "o:family:high": {},
        "h:family:high": {},
    }
zranitelny_vendor = {}
zranitelny_vendor_high = {}

# Typ produktu na citelnou podobu
typ_friendly = {
    "a": "Aplikace",
    "o": "OS",
    "h": "Hardware",
    "a:family": "Aplikace (rodina)",
    "o:family": "OS (rodina)",
    "h:family": "Hardware (rodina)",
    "a:family:high": "Aplikace (rodina, high)",
    "o:family:high": "OS (rodina, high)",
    "h:family:high": "Hardware (rodina, high)"
}

# Dokumentace: https://en.wikipedia.org/wiki/Common_Platform_Enumeration
# CPE 2.3 URI ma treba podobu:
# cpe:2.3:a:intel:integrated_performance_primitives_cryptography:2019:-:*:*:*:*:*:*
def cpe23UriDekoder(cpe23Uri, severity):
    chops = cpe23Uri.split(":")
    typ = chops[2]
    vendor = chops[3]
    productfamily = chops[4]
    version = chops[5]
    if version == "*" or version == "-":
        version = ""
    else:
        version = f" {version}" 
                                
    productfull = vendor + " " + productfamily+version
    productfamilyfull = vendor + " " + productfamily

    # Evidence vendoru/firem                       
    if vendor in zranitelny_vendor:
        zranitelny_vendor[vendor] += 1
    else:
        zranitelny_vendor[vendor] = 1

    # Evidence produktu s uvedenim verze (treba Windows 10.124 atp.)
    if productfull in zranitelny_produkt[typ]:
        zranitelny_produkt[typ][productfull] += 1
    else:
        zranitelny_produkt[typ][productfull] = 1

    # Evidence produktu bez uvedeni verze (treba Windows 10 atp.)
    if productfamilyfull in zranitelny_produkt[typ+":family"]:
        zranitelny_produkt[typ+":family"][productfamilyfull] += 1
    else:
        zranitelny_produkt[typ+":family"][productfamilyfull] = 1

    # Evidence zranitelnych produktu s nevyssi zavaznosti HIGH az CRITICAL
    # Podle skore CVSS v2.0 nebo CVSS v3.0
    # https://nvd.nist.gov/vuln-metrics/cvss
    if severity >= 7.0:
        if vendor in zranitelny_vendor_high:
            zranitelny_vendor_high[vendor] += 1
        else:
            zranitelny_vendor_high[vendor] = 1

        if productfamilyfull in zranitelny_produkt[typ+":family:high"]:
            zranitelny_produkt[typ+":family:high"][productfamilyfull] += 1
        else:
            zranitelny_produkt[typ+":family:high"][productfamilyfull] = 1


if __name__ == "__main__":

    print("")
    print("**************** STATISTIKA NEJDERAVEJSICH PRODUKTU ****************")
    print("* Lze pouzit s JSON soubory z https://nvd.nist.gov/vuln/data-feeds *")
    print("********************************************************************")
    print("")

    if len(sys.argv) != 2:
        print("Pouziti: python vycmuchejto.py nvdcve-xxxx.json")
        print("")
    else:
    
        fn = sys.argv[1]
        delka_zebricku = 10 # Delka zebricku pro vypis
        n_kriticke = 0
        t0 = perf_counter()
        print(f"Nacitam soubor {fn}... ", end="", flush=True)
        with open(fn, "r") as f:
            root = json.load(f)
            print("OK")
            zranitelnosti = root["CVE_Items"]
            pocet_zranitelnosti = len(zranitelnosti)
            n = 0
            print(f"Celkem zranitelnosti: {pocet_zranitelnosti}")
            print("")

            # Projdi vsechny CVE
            for i, zranitelnost in enumerate(zranitelnosti):
                cve_id = zranitelnost["cve"]["CVE_data_meta"]["ID"]
                cve_zavaznost_skore = 0
                if "baseMetricV3" in zranitelnost["impact"]:
                    cve_zavaznost_skore = zranitelnost["impact"]["baseMetricV3"]["cvssV3"]["baseScore"]
                else:
                    if "baseMetricV2" in zranitelnost["impact"]:
                        cve_zavaznost_skore = zranitelnost["impact"]["baseMetricV2"]["cvssV2"]["baseScore"]

                if cve_zavaznost_skore >= 7.0:
                    n_kriticke += 1

                nodes = zranitelnost["configurations"]["nodes"]
                uris = []
                for node in nodes:

                    # Iteruj vsechny vnorene deti
                    if "children" in node:
                        children = node
                        while "children" in children:
                            children = children["children"]
                            for child in children:
                                records = child["cpe_match"]
                                for record in records:
                                    if record["vulnerable"] == True:
                                        uris.append(record["cpe23Uri"])

                    # Bezdetny zbytek
                    records = node["cpe_match"]
                    for record in records:
                        if record["vulnerable"] == True:
                            uris.append(record["cpe23Uri"])
                    
                # Zpracuj nalezene CPE 2.3 URI
                n += len(uris)
                nn = len(uris)
                for uri in uris:
                    cpe23UriDekoder(uri, cve_zavaznost_skore)
                poradi = str(i+1).rjust(5)+"/"+str(pocet_zranitelnosti)
                print(f"{poradi} {cve_id.ljust(14)}: Produktu: {str(nn).ljust(4)} (Celkem: {n})", end="\r")

            print("\n\n")

            print(f"Kritickych chyb se skorem 7,0+: {n_kriticke}")
            print(f"Produktu (aplikace, OS, hardware): {n}")
            print("")

            # Serazeni firem podle hodnoty sestupne
            zranitelny_vendor = dict(sorted(zranitelny_vendor.items(), key=lambda item: item[1], reverse=True))
            zranitelny_vendor_high = dict(sorted(zranitelny_vendor_high.items(), key=lambda item: item[1], reverse=True))

            # Serazeni produktu podle hodnoty sestupne
            for typ in zranitelny_produkt:
                zranitelny_produkt[typ] = dict(sorted(zranitelny_produkt[typ].items(), key=lambda item: item[1], reverse=True))

            
            # Ulozeni do CSV a vypis n nejzranitelnejsich produktu
            for typ in zranitelny_produkt:
                print(f"TOP{delka_zebricku} {typ_friendly[typ]} podle zranitelnosti:")
                print("===================================================")
                f = open(f"{typ_friendly[typ]}.csv", "w")
                i = 1
                for produkt, pocet in zranitelny_produkt[typ].items():
                    if i <= delka_zebricku:
                        print(f"{str(i).rjust(2)}) {produkt}: {pocet}")
                    f.write(f"{produkt};{pocet}\n")
                    i += 1 
                f.close()
                print("")

            # Ulozeni do CSV a vypis n nejzranitelnejsich firem
            print(f"TOP{delka_zebricku} Firmy podle zranitelnosti:")
            print("===================================================")
            f = open("Firmy.csv", "w")
            i = 1
            for vendor, pocet in zranitelny_vendor.items():
                if i <= delka_zebricku:
                    print(f"{str(i).rjust(2)}) {vendor}: {pocet}")
                f.write(f"{vendor};{pocet}\n")
                i += 1
            f.close()
            print("")

            # Ulozeni do CSV a vypis n nejzranitelnejsich firem se zranitelnosti high
            print(f"TOP{delka_zebricku} Firmy podle nejzavaznejsich zranitelnosti high:")
            print("===================================================")
            f = open("Firmy_high.csv", "w")
            i = 1
            for vendor, pocet in zranitelny_vendor_high.items():
                if i <= delka_zebricku:
                    print(f"{str(i).rjust(2)}) {vendor}: {pocet}")
                f.write(f"{vendor};{pocet}\n")
                i += 1
            f.close()
            print("")

            t1 = perf_counter()
            print(f"Analyza trvala {(t1-t0)} sekund")
            print("")
