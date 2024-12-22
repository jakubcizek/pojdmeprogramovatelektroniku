#define MAGIC_VALUE 0xAABB
#define EEPROM_SIZE 128

// Funkce pro kontrolu, zda je EEPROM připravena (obsahuje magickou hodnotu)
bool isEEPROMReady() {
  uint16_t magicCheck = (EEPROM.read(0) << 8) | EEPROM.read(1);
  return magicCheck == MAGIC_VALUE;
}

// Funkce pro vymazání celé EEPROM
void clearEEPROM() {
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

// Funkce pro zápis magického čísla na adresu 0
void writeEEPROMMagic() {
  EEPROM.write(0, (MAGIC_VALUE >> 8) & 0xFF);
  EEPROM.write(1, MAGIC_VALUE & 0xFF);
  EEPROM.commit();
}

// Funkce pro zápis hodnoty typu uint32_t na libovolnou adresu
void setU32EEPROM(int address, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    EEPROM.write(address + i, (value >> (i * 8)) & 0xFF);
  }
  EEPROM.commit();
}

// Funkce pro čtení hodnoty typu uint32_t z libovolné adresy
uint32_t getU32EEPROM(int address) {
  uint32_t value = 0;
  for (int i = 0; i < 4; i++) {
    value |= (EEPROM.read(address + i) << (i * 8));
  }
  return value;
}