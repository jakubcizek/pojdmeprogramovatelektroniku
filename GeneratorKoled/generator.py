# Generator projektu pro Arduino Uno a jine AVR cipy z hudebniho MIDI souboru
# Ton se na Arduin ugeneruje pomoci funcke tone na GPIO 3

# Pouziti: python3 generator.py soubor.mid

# Knihovna pro dekodovani MIDI
# Pip: https://pypi.org/project/pretty_midi/
# GitHub: https://github.com/craffel/pretty-midi
# Dokumentace: http://craffel.github.io/pretty-midi/
import pretty_midi
import sys

# Nazev nastroje, jehoz noty budeme prevadet na frekvence
# Pokud ho nenajdeme, pouzijme prvni instrument v poradi
hudebniNastroj = "Melody"


if len(sys.argv) != 2:
    print("Pouziti: generator.py soubor.mid")
    
else:
    koledaSoubor = sys.argv[1]

    # Nacteni konverzni tabulky pro prevod MIDI nota -> frekvence
    # Stazeno z webu www.inspiredacoustics.com/en/MIDI_note_numbers_and_center_frequencies
    frekvence = []
    with open("frekvencetonu.txt", "r") as f:
        frekvence = f.read().split("\n")

    # Jen pro zajimavost, tempo v kodu jinak nikde nepouzivame
    print(f"Analyzuji koledu {koledaSoubor} ...\n")
    midi = pretty_midi.PrettyMIDI(koledaSoubor)
    print(f"Průměrné tempo: {midi.estimate_tempo():.0f} BPM")

    # Vypis hudebnich nastroju ve sklade
    print("\nV koledě se nacházejí tyto nástroje")
    melodie = midi.instruments[0]
    for instrument in midi.instruments:
        nazev = instrument.name
        if len(nazev) == 0:
            nazev = "Bez názvu"
        print(f" 🎵 {nazev}")
        if hudebniNastroj in instrument.name:
            melodie = instrument

    print("")

    if len(melodie.name) == 0:
        melodie.name = "Bez názvu"

    if hudebniNastroj not in melodie.name:
        print(f"Nemohu najít nástroj {hudebniNastroj}")
        print(f"Použiji první v pořadí: {melodie.name}")
    
    # Precteme noty zvoleneho nastroje
    arduinoPole = []
    print(f"Generuji pole pro Arduino z nástroje {melodie.name}... ", end="")

    # Projdeme vsechny noty nastroje, prevedeme je na noty a spocitame casy zvuk ua ticha
    # Vysledkem bude dvurozmerny list arduinoPole, ktery predstavuje seznam listu [frekvence, delka_v_ms]
    if melodie.notes[0].start > 0:
        arduinoPole.append([0, int(melodie.notes[0].start * 1000)])

    for i in range(1, len(melodie.notes)):

        predchozi = melodie.notes[i-1]
        aktualni = melodie.notes[i]
            
        frek = frekvence[predchozi.pitch]
        delka = int(predchozi.get_duration() * 1000)
        arduinoPole.append([frek, delka])

        prodlevaMezi = int(aktualni.start * 1000) - int(predchozi.end * 1000)
        if prodlevaMezi > 0:
            arduinoPole.append([0, prodlevaMezi])
        
    posledni = melodie.notes[len(melodie.notes)-1]
    arduinoPole.append([frekvence[posledni.pitch], int(posledni.get_duration() * 1000)])

    # Vygenerujeme hlavickovy kod pro Arduino C/C++
    # s polem frekvenci a delek tonu
    kod = f"// {koledaSoubor}\n"
    kod += "// Pole pro funkci tone()\n"
    kod += "// Pole se nacita z flashove pameti a setri RAM\n"
    kod += "// Vnorene pole obsahuje frekvenci tonu a delku tonu v ms\n\n"
    kod += "#include <avr/pgmspace.h>\n\n"
    kod += f"const uint16_t delkaKoledy = {len(arduinoPole)};\n"
    kod += f"const uint16_t koleda[{len(arduinoPole)}][2] PROGMEM = {{\n"
    for i, ton in enumerate(arduinoPole):
        kod += f"    {{{ton[0]},{ton[1]}}}"
        if(i < len(arduinoPole)-1):
            kod += ",\n"
    kod += "\n};\n"

    print("HOTOVO")

    # Ulozime havickovy kod do souboru podle nazvu uvodni koledy
    print(f"Generuji soubor {koledaSoubor}.h... ", end="")
    with open(koledaSoubor + ".h", "w") as h:
        h.write(kod)
    print("HOTOVO")

    # Vygenerujeme hlavni kod programu
    print(f"Generuji soubor prehravac.ino... ", end="")
    kod = f"#include \"{koledaSoubor}.h\""
    kod += """
void setup() {
    ;
}

void loop() {
    for (uint16_t i = 0; i < delkaKoledy; i++) {
        uint16_t ton = pgm_read_word(&(koleda[i][0]));
        int16_t delka = pgm_read_word(&(koleda[i][1]));
        if(ton > 0) tone(3, ton);
        else noTone(3);
        delay(delka);
    }
    noTone(3);
    delay(1000);
}"""

    # Ulozime hlavni kod programu do souboru prehravac.ino
    with open("prehravac.ino", "w") as ino:
        ino.write(kod)
        
    print("HOTOVO\n")