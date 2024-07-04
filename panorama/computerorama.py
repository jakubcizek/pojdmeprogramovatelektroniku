# Nezapomeň nainstalovat použité knihovny:
# pip install opencv-python
# pip install pillow
# pip install tkinterdnd2
# pip install numpy

import tkinter as tk
from tkinter import ttk
from tkinter import filedialog
from tkinterdnd2 import DND_FILES, TkinterDnD
from PIL import Image, ImageTk
import numpy as np
import cv2
import os

# Třída GUI okna,kterou později spustíme
# Je v ní kompeltní logika naší aplikace
class GeneratorPanorama(TkinterDnD.Tk):
    # Funkce init se zparacuje při inicializaci třídy
    def __init__(self):
        super().__init__()

        # Nadpis a rozměry okna, které bude mít neměnnou velikost
        self.title("Computerorama 1.0: Generátor panoramat")
        self.geometry("1280x720")
        self.resizable(False, False)
        self.velikost_miniatur = 250

        # Vytvoříme zákaldní objekty okna a horizontální posuvník,
        # který bude sloužit pro posouvání pásu s miniaturami dílků pro panorama,
        # pokud se nevejdou do viditelné části okna
        self.platno = tk.Canvas(self, borderwidth=0)
        self.ram = tk.Frame(self.platno)
        self.posuvnik = ttk.Scrollbar(self, orient="horizontal", command=self.platno.xview)
        self.platno.configure(xscrollcommand=self.posuvnik.set)

        # Rozmístíme základní objekty v okně; posuvník bude při spodním okraji,
        # platno pro miniatury pak bude nahoře
        self.posuvnik.pack(side="bottom", fill="x")
        self.platno.pack(side="top", fill="both", expand=True)
        self.platno.create_window((4, 4), window=self.ram, anchor="nw", tags="self.frame")
        self.ram.bind("<Configure>", self.priKonfiguraciRamu)

        # Při přetažení souboru do okna se zavolá funkce priDragAndDropu
        self.drop_target_register(DND_FILES)
        self.dnd_bind("<<Drop>>", self.priDragAndDropu)

        # Vytvoříme si pole a proměnné po obrázky
        self.obrazky_gui = [] # Pole s GUI prvky miniatur v okně
        self.obrazky = []  # Pole s originálními obrázky a popisnými informacemi ke každém uz nich
        self.panorama = None  # Paměť pro vytvořené panorama
        self.panorama_ram = None # GUI prvek s miniaturou panoramatu, které zobrazíme v okně

        # Přidáme hlavní menu do horní lišty okna
        self.menubar = tk.Menu(self)
        self.config(menu=self.menubar)

        # Přidáme položky do menu a funkce, které se spustí po klepnutí
        self.hlavni_menu = tk.Menu(self.menubar, tearoff=0)
        self.hlavni_menu.add_command(label="Složit panorama", command=self.slozitPanorama)
        self.hlavni_menu.add_command(label="Začít znovu", command=self.vsechnoSmazat)
        self.hlavni_menu.add_command(label="Uložit", command=self.save_panorama)
        self.menubar.add_cascade(label="Ovládání", menu=self.hlavni_menu)

        # Přidáme tučný informační popisek na střed okna
        self.hlavni_popisek = tk.Label(self, text="Přetáhněte do okna obrázky pro panorama", font=("Helvetica", 24), fg="grey")
        self.hlavni_popisek.place(relx=0.5, rely=0.5, anchor=tk.CENTER)

    # Pomocná funkce, která se zavolá při konfiguraci rámu slouží pro nastavení posuvníku
    def priKonfiguraciRamu(self, event):
        self.platno.configure(scrollregion=self.platno.bbox("all"))

    # Funkce, která se zavolá při přetažení souboru do okna
    def priDragAndDropu(self, udalost):
        # Ziskame pole s přetaženými soubory
        soubory = self.tk.splitlist(udalost.data)
        # Projdeme soubor po souboru
        for soubor in soubory:
            # Pokud má soubor příponu PNG, JPG nebo JPEG, předpokládáme,
            # že to je obrázek a zavoláme funkci pridejObrazek
            if soubor.lower().endswith((".png", ".jpg", ".jpeg")):
                self.pridatObrazek(soubor)

        # Po každém přidaném obrázku seřadíme jejich pole podle názvu souboru
        # a poté zavoláme funkci pro jejich zobrazení v okně
        self.obrazky.sort(key=lambda x: x["nazev"])
        self.zobrazitObrazky()

    # Funkce pro přidání obrázku
    def pridatObrazek(self, cesta):
        # Nahrajeme obrázek, převedeme jej do formátu OpenCV a zároveň vytvoříme 100px miniaturu
        obrazek = Image.open(cesta)
        sirka, vyska = obrazek.size
        obrazek_opencv = np.array(obrazek)
        obrazek_opencv = cv2.cvtColor(obrazek_opencv, cv2.COLOR_RGB2BGR)
        obrazek.thumbnail((self.velikost_miniatur, self.velikost_miniatur), Image.Resampling.LANCZOS)
        obrazek = ImageTk.PhotoImage(obrazek)

        # Uložíme obrázek a jeho popisné informace do pole
        self.obrazky.append({
            "nazev": os.path.basename(cesta),
            "cesta": cesta,
            "miniatura": obrazek,
            "original": obrazek_opencv,
            "rozmery": f"{sirka}x{vyska}"
        })
        print(f"Nahrál jsem obrázek {cesta} s rozměry {sirka}x{vyska}")

    # Funkce pro zobrazení obrázků v okně ve formě GUI prvků
    def zobrazitObrazky(self):
        # Smažeme veškleré GUI objekty v rámu pro obrázky
        for widget in self.ram.winfo_children():
            widget.destroy()

        # Projdeme pole obrázků
        for obrazek in self.obrazky:
            # Vytvoříme rám pro obrázek a jeho textový popisek
            ram = tk.Frame(self.ram)
            ram.pack(side="left", padx=5, pady=5)

            # Vložíme do rámu miniaturu
            miniatura = tk.Label(ram, image=obrazek["miniatura"])
            miniatura.image = obrazek["miniatura"]
            miniatura.pack()

            # A poté do rám uvložíme ještě popisek
            popisek = f"{obrazek['nazev']}, {obrazek['rozmery']}"
            popisek = tk.Label(ram, text=popisek, font=("Helvetica", 10))
            popisek.pack()

            # Rám s miniaturou vložíme do pole pro GUI prvky s moniaturami
            self.obrazky_gui.append(ram)

        # Pokud máme v poli alespoň jednu miniaturu, změníme hlavní popisek  vokně
        if len(self.obrazky_gui) > 0:
            self.hlavni_popisek.config(text="Výborně, přidávej další obrázky dle libosti")

    #Funcke pro reset projektu
    def vsechnoSmazat(self):
        # Smažeme veškleré GUI objekty v rámu pro obrázky
        for widget in self.ram.winfo_children():
            widget.destroy()

        # Smažeme GUI objekt s miniaturou panoramatu, pokud už existuje
        if self.panorama_ram:
            self.panorama_ram.destroy()
            self.panorama_ram = None

        # Smažeme všechna pole s obrázky a paměť na panorama
        self.obrazky_gui = []
        self.obrazky = []
        self.obrazky = []
        self.panorama = None

        # Resetujeme hlavní popisek do výchozí podoby
        self.hlavni_popisek.config(text="Přetáhněte do okna obrázky pro panorama")

    # Funkce pro složení panoramatu
    def slozitPanorama(self):
        # Smažeme GUI objekt s miniaturou panoramatu, pokud už existuje
        if self.panorama_ram:
            self.panorama_ram.destroy()
            self.panorama_ram = None

        # Nastavíme nový hlavní popisek
        self.hlavni_popisek.config(text="Skládám panorama! Může to chvíli trvat...")
        self.update_idletasks() # Vynutíme si dokončení všech GUI změn, protože teď budeme hodně dlouho blokovat

        # Vytvoříme instanci automatického skládače panoramat z knihovny OpenCV
        stitcher = cv2.Stitcher_create()
        # Pošleme do něj všechny originály nahranných fotek a necháme jej pracovat
        # Skládání dle počtu a veliksoti obrázků a výkon uvašeho PC může zabrat nějaký čas
        # Pro jednoduchost příkladu jsme neimplementovali žádné otáčející se kolečko apod.
        status, self.panorama = stitcher.stitch([polozka["original"] for polozka in self.obrazky])

        # Pokud se podařilo složit panorama
        if status == cv2.Stitcher_OK:
            # Smažeme hlavní popisek
            self.hlavni_popisek.config(text="")
            # Převedeme obrázek panoramatu z BGR do RGB (OpenCV pracuje v BGR) pro účely vytvoření GUI miniatury
            miniatura = cv2.cvtColor(self.panorama, cv2.COLOR_BGR2RGB)
            miniatura = Image.fromarray(miniatura)

            # Zmenšíme panorama na 95% šířku okna (nebo na výšku poloviny okna) a převedeme ji na formát obrázku pro GUI
            sirka = self.winfo_width()
            sirka = int(sirka * 0.95)
            miniatura.thumbnail((sirka, (self.winfo_height() / 2) - 10), Image.Resampling.LANCZOS)
            miniatura = ImageTk.PhotoImage(miniatura)

            # Vložíme miniaturu do okna do spodní části s malým odstupem
            self.panorama_ram = tk.Frame(self)
            self.panorama_ram.pack(side="bottom", fill="x", expand=True)
            panorama_gui = tk.Label(self.panorama_ram, image=miniatura)
            panorama_gui.image = miniatura
            panorama_gui.pack(expand=True)

        # V opačné mpřípadě zobrazíme chybové hlášení
        else:
            # Pokud nemá Stitcher dostatek obrázků
            if status == cv2.Stitcher_ERR_NEED_MORE_IMGS:
                self.hlavni_popisek.config(text="Ke složení panoramatu chybí dost vhodných obrázků")
                tk.messagebox.showwarning("Chyba", "Ke složení panoramatu chybí dost vhodných obrázků")
            # Pokud se nepodařila homografie = identifikace společných klíčovacích bodů   
            elif status == cv2.Stitcher_ERR_HOMOGRAPHY_EST_FAIL:
                self.hlavni_popisek.config(text="Výpočet homografie selhal")
                tk.messagebox.showwarning("Chyba", "Výpočet homografie selhal. Opravdu se jedná o dílky panoramatického snímku?")
            else:
                self.hlavni_popisek.config(text="Nemohu složit panorama a nevím proč")
                tk.messagebox.showwarning("Chyba", "Nemohu složit panorama a nevím proč")
            self.vsechnoSmazat()

    # Funkce pro uložení panoramatického snímku v plném rozlišení
    def save_panorama(self):
        if self.panorama is not None:
            cesta = filedialog.asksaveasfilename(defaultextension=".jpg", filetypes=[("JPEG files", "*.jpg"), ("PNG files", "*.png"), ("All files", "*.*")])
            print(cesta)
            if cesta:
                ulozeno = cv2.imwrite(
                    cesta, 
                    self.panorama,
                    [cv2.IMWRITE_JPEG_QUALITY, 90]
                )
                if ulozeno == False:
                    print(f"Nemohu uložit soubor {cesta}")
                    tk.messagebox.showwarning("Chyba", "Nelze uložit soubor do tohoto adresáře") 

            else:
                print(f"Nemohu uložit soubor {cesta}")
                tk.messagebox.showwarning("Chyba", "Nelze uložit soubor do tohoto adresáře") 
        else:
            print("Nemohu uložit soubor. Panorama musíte nejprve vytvořit")
            tk.messagebox.showwarning("Chyba", "Nejprve složte panorama")

# Faktický začátek běhu našeho programu
if __name__ == "__main__":
    # Vytvoříme instanci naší třídy s GUI oknem a spustíme jeho hlavní interní smyčku
    aplikace = GeneratorPanorama()
    aplikace.mainloop()
