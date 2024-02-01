G21                    ; Milimetrove souradnice
G90                    ; Absolutni souradnice
G28 W                  ; Home all az na mereni roviny
G0 Z30                 ; Vyjed do vysky 30 mm
G0 X168 Y148 F10000    ; Bez na stred desky (rozmery pro MK3S + 43 offset tuzky)
G0 Z2.0 F5000          ; Vyska kresleni 2,0 mm
G0 X218                ; Nakresli ctyri strany ctverce 50x50 mm
G0 Y198
G0 X168
G0 Y148
G2 I25 J25              ; Nakresli okolo ctverce kolecko
G0 Z30                  ; Vyjed do bezpecne vysky 30 mm
G0 X0 Y200 F50000       ; Zajed na 0;200 (tiskova deska vyjede pred nas)
M84             	      ; Vypnout motory a nezapomenout na zalomeni radku
