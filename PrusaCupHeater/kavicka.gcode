; *** OHRIVAC KAFE S MICHANIM ***
G21			              ; Milimetrovy system
G90			              ; Absolutni souradnicovy system
G28 W			            ; Home all az na bodove mereni; Pozor, klicovy parametr W podporuje Prusa, ale nemusi to byt samozrejmost u ostatnich. Tiskarna nemsi proves bodobvou kalibraci, protoze je na ni pripevnena lzicka
G0 Z170 Y200 F50000 	; Vyjed hlavou do vysky 17 cm a plosinou vpred; bezpecna vyska pro pouzity hrnek
M140 S60 		          ; Ohrej desku na 60 stupnu
M190     		          ; Cekej na ohrati
;
; *** TED POLOZ NA STRED HRNEK ***
;
G4 S5			            ; Cekej 5 sekund pro polozeni hrnku
G0 X155 Y142		      ; Zajed lzickou na stred na stred
G0 Z80			          ; Sjed hlavou do vysky 8 cm, lzicka v kafe. POZOR, ZALEZI NA VASI LZICCE A UCHOPU. OPATRNE!!!
G0 F2000		          ; Zpomalime rychlost pohybu
G2 I12 J12 		        ; Proved tri krouzive pohyby s polomerem 1,2 cm
G2 I12 J12 
G2 I12 J12 
G0 Z170               ; Vyjed do 15 cm
G4 S10                ; Cekej 10 sekund
G0 Z80                ; Opet sjed do michaci vysky a proved tri krouzive pohyby
G2 I12 J12 
G2 I12 J12 
G2 I12 J12 
G0 F50000             ; Zrychlime rychlost pohybu
G0 Z170               ; Vyjed do bezpecne vysky. Kafe je zamichane
G0 Y200               ; Vyjed s plosinou dopredu
G4 S1800              ; Cekej dalsich 30 minut
M140 S0               ; Kazdy normalni clovek uz kafe dopil, takze vychladni
