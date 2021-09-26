# Nazev hotoveho programu
EXE = 3dlidar

# I2C sbernice (I2C0, nebo I2C1)
I2C = I2C1

# Prekladac C++
COMPILER = g++

#ADRESARE PROJEKTU
APP = ./app/
SDK = ./sdk/
OUT = ./out/

# Externi knihovny

# OpenCV
EXTERNAL_LIBS += $$(pkg-config --libs opencv)

# Parametry C/C++ prekladace
# -Wall, -Wextra ... zobrazuj vetsinu varovani
# -Wfatal-erros  ... pri zavazne chybe ukonci dalsi preklad
# -Os            ... oprimalizuj preklad pro nejkratsi kod
# -g0            ... bez dat pro debugging (release build)
# -D I2C0/I2C1   ... makro nastavujici I2C sbernici v SDK/platform/platform.c
FLAGS = -Wall -Wextra -Wfatal-errors -Os -g0 -D $(I2C) $(EXTERNAL_LIBS)

# Adresare s hlavickovymi soubory .h
INCLUDE_PATH += -I $(SDK)uld-driver/inc
INCLUDE_PATH += -I $(SDK)platform
INCLUDE_PATH += -I $(APP)

# Zdrojove soubory projektu .c a .cpp
CPP_SOURCES += $(wildcard $(SDK)uld-driver/src/*.c)
CPP_SOURCES += $(wildcard $(SDK)platform/*.c)
CPP_SOURCES += $(wildcard $(APP)*.cpp)

# Konecna podoba prikazu k prekladu
COMMAND = $(COMPILER) $(INCLUDE_PATH) $(FLAGS) -o $(OUT)$(EXE) $(CPP_SOURCES)

help:	
	@echo "----------------------------------------------"
	@echo "| Vita vas projekt 3D LiDAR pro Raspberry Pi |"
	@echo "----------------------------------------------"
	@echo ""
	@echo "Prikazy:"
	@echo " make all   = Vytvori spustitelny soubor $(OUT)$(EXE)"
	@echo " make clean = Smaze obsah adresare $(OUT)"
	@echo " make help  = Zobrazi tuto napovedu"
	@echo ""
	@echo "Prekladac: $(COMPILER)"
	@echo "Parametry prekladace: $(FLAGS)"
	@echo "Zdrojove soubory aplikace: $(APP)"
	@echo "Zdrojove soubory SDK: $(SDK)"
	@echo "Zdrojove soubory HAL: $(SDK)platform"
	@echo "Zdrojove soubory ULD: $(SDK)uld-driver"
	@echo ""
	@echo "Pouzity hardware:"
	@echo " Raspberry Pi 4"
	@echo " VL53L5CX-SATEL na sbernici $(I2C)"
	@echo ""
	@echo "Pouzity software: Raspberry Pi OS"
	@echo ""
	@echo "Pouzite knihovny:"
	@echo " libopencv-dev 3.2.0+dfsg-6 armhf"
	@echo " (Instalace: sudo apt install libopencv-dev)"
	@echo ""
	@echo "Vygenerovany kompletni prikaz k prekladu:"
	@echo ""
	@echo "$(COMMAND)"

all:
	$(info Spoustim preklad...)
	$(info Hotovy program najdete v $(OUT)$(EXE))
	$(info ----------------------------------------------------------)
	$(COMMAND)

clean:
	$(info Mazu soubory v adresari $(OUT))
	rm -f $(OUT)*
