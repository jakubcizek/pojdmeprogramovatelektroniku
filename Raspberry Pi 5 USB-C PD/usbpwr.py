#!/usr/bin/env python3
import struct
import os

PDO_FILE = "/proc/device-tree/chosen/power/usbpd_power_data_objects"

def parse_pdo(pdo_value):
    if pdo_value == 0:
        return None
    
    pdo_type = (pdo_value >> 30) & 0x03
    
    # Prevod cele 32bitove hodnoty na binarni string doplneny nulami zleva
    bin_str = f"{pdo_value:032b}"
    # Rozdeleni stringu do logickych bloku pro lepsi citelnost:
    # 2 bity (Typ) | 10 bitu (Flagy) | 10 bitu (Napeti) | 10 bitu (Proud)
    bin_formatted = f"{bin_str[:2]} {bin_str[2:12]} {bin_str[12:22]} {bin_str[22:]}"
    
    if pdo_type == 0:  # Fixed Supply
        voltage_raw = (pdo_value >> 10) & 0x3FF
        current_raw = pdo_value & 0x3FF
        
        voltage_v = voltage_raw * 0.05
        current_a = current_raw * 0.01
        
        breakdown = []
        breakdown.append("Bity 31-30 (Typ profilu): 00 (Fixed Supply)")
        
        if (pdo_value >> 29) & 1: breakdown.append("Bit  29    (Flag):        1 (Dual-Role Power - umi prijimat energii)")
        if (pdo_value >> 28) & 1: breakdown.append("Bit  28    (Flag):        1 (USB Suspend Supported)")
        if (pdo_value >> 27) & 1: breakdown.append("Bit  27    (Flag):        1 (Unconstrained Power)")
        if (pdo_value >> 26) & 1: breakdown.append("Bit  26    (Flag):        1 (USB Comms Capable - datova komunikace)")
        if (pdo_value >> 25) & 1: breakdown.append("Bit  25    (Flag):        1 (Dual-Role Data - umi Host i Device)")
        if (pdo_value >> 24) & 1: breakdown.append("Bit  24    (Flag):        1 (Unchunked Extended Messages)")
        if (pdo_value >> 23) & 1: breakdown.append("Bit  23    (Flag):        1 (EPR Mode Capable - profily nad 100 W)")
        
        peak_current = (pdo_value >> 20) & 0x07
        if peak_current > 0:
            breakdown.append(f"Bity 22-20 (Peak Curr.):  {peak_current} (profil spickoveho odberu)")
            
        breakdown.append(f"Bity 19-10 (Napeti):      {voltage_raw} * 50 mV = {voltage_v:.2f} V")
        breakdown.append(f"Bity  9-0  (Proud):       {current_raw} * 10 mA = {current_a:.2f} A")
        
        return {
            "hex": f"{pdo_value:08x}",
            "bin": bin_formatted,
            "type": "Fixed",
            "voltage_V": voltage_v,
            "current_A": current_a,
            "breakdown": breakdown
        }
    return {"hex": f"{pdo_value:08x}", "bin": bin_formatted, "type": "Other"}

def main():
    if not os.path.exists(PDO_FILE):
        print(f"Chyba: Soubor {PDO_FILE} neexistuje.")
        return

    print("Vyjednany profil z USB-C PD zdroje (nejde o mereni spotreby):")
    print("-" * 65)
    
    try:
        with open(PDO_FILE, "rb") as f:
            while True:
                chunk = f.read(4)
                if not chunk or len(chunk) < 4:
                    break
                
                pdo_value = struct.unpack(">I", chunk)[0]
                parsed = parse_pdo(pdo_value)
                
                if parsed:
                    if parsed["type"] == "Fixed":
                        print(f"Napeti: {parsed['voltage_V']:.2f} V")
                        print(f"Proud:  {parsed['current_A']:.2f} A\n")
                        
                        print(f"Surova data (HEX): 0x{parsed['hex']}")
                        print(f"Surova data (BIN): {parsed['bin']}  (Typ | Flagy | Napeti | Proud)")
                        
                        for line in parsed["breakdown"]:
                            print(f"- {line}")
                    else:
                        print("Profil: Jiny (ne Fixed Supply)")
                        print(f"Surova data (HEX): 0x{parsed['hex']}")
                        print(f"Surova data (BIN): {parsed['bin']}")
                        
                    print("-" * 65)
                    
    except Exception as e:
        print(f"Chyba: {e}")

if __name__ == "__main__":
    main()
