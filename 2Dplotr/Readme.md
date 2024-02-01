## 2D plotr z 3D tiskárny

Přílohy pro článek v časopisu Computer 3/24
 -  plotr.toml: Profil pro konvertor vektorových dat na G-code vpype a vpype-gcode (upravit výšku kreslení podle aktuální situace)
 -  ctverecek.gcode: Komentovaný G-code pro kresbu čtverečku obtaženého kružnicí ve výšce 2.0 mm nad pvorchem a na tiskové ploše s rozměry Prusa MK3S+

Příkaz pro konverzi souboru vlny.svg na vlny.gcode:

(Numerické hodnoty 86mm a 43mm opravit dle toho, kde se oproti trysce nachází tužka - posun)

vpype --config .\plotr.toml read .\vlny.svg scale --origin 0 86mm -- 1 -1 translate 43mm 43mm linemerge linesort gwrite --profile tvrdypapir vlny.gcode
