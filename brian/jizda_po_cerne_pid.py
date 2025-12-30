import brian.sensors as sensors # Knihovna pro ovládání čidel
import brian.motors as motors # Knihovna pro ovládání motorů
from time import sleep


# Inicializace čidel
print("Inicializuji senzory... ", end="")
color = sensors.EV3.ColorSensorEV3(sensors.SensorPort.S1)
color.wait_until_ready()
touch = sensors.EV3.TouchSensorEV3(sensors.SensorPort.S2)
touch.wait_until_ready()
print("OK")

# Inicializace předních motorů L/R (levý/pravý)
print("Inicializuji motory... ", end="")
motor_l = motors.EV3LargeMotor(motors.MotorPort.A)
motor_l.wait_until_ready()
motor_r = motors.EV3LargeMotor(motors.MotorPort.B)
motor_r.wait_until_ready()
print("OK")

# Stavy programu, které nastavujeme stiskem tlačítka na zádi
STATE_RUNNING = 1
STATE_STOP = 0
state = STATE_STOP

# Úvodní kalibrace černého a bílého povrchu na herním poli s čárou,
# po které se bude roboto navigovat 
print("Poloz na CERNOU a stiskni tlacitko")
touch.wait_for_press_and_release()
sleep(.5) # Drobný debouncing
black_val = color.reflected_value()
print(f"Cerna: {black_val}")

print("Poloz na BILOU a stiskni tlacitko")
touch.wait_for_press_and_release()
sleep(.5) # Drobný debouncing
white_val = color.reflected_value()
print(f"Bila: {white_val}")

# Cílová hodnota odrazivosti ja eritmetický průměr mezi změřeno učerno ua bílou barvou
# To bude hodnota, které se bude robot držet při navigaci po hraně černé linky
target = (black_val + white_val) / 2
print(f"TARGET: {target}")

# Parametry Kp/Ki/Kd regulátoru PID pro jemný pohyb po černé číře
# Viz https://cs.wikipedia.org/wiki/PID_regul%C3%A1tor a další zdroje
# Toto jsou výchozí parametry, které je možné dále ladit podle rychlosti, charakteru dráhy aj.
# S laděním algoritmu PID pomůže každý AI chatbot, je to totiž zákaldní disciplína oboru automatizace 
Kp = 5 # Proporoční složka
Ki = 0 # Integrační složka
Kd = 0 # Derivační složka

# Základní/bazální rychlost, kterou dále upravuje algoritmus PID
# Při rychlosti 100 bude robot velmi pomalý, ale zase bude fungovat = nevyjede kvůli špatně odladěnému PID z dráhy
base = 100

# Pomocné hodnoty, které si PID nastavuje při jízdě „učením“ přechodu černé a bílé
integral = 0
last_error = 0

# Čekáme na stisk tlačítka pro start, poté roztočíme motory
print("Nastartuj tlacitkem")

# Nekonečná smyčka
while True:
    # Pokud jsme stiskli tlačítko, podle aktuálního stavu zastavíme, nebo opět roztočíme motory
    if touch.is_pressed():
        if state == STATE_RUNNING:
            motor_l.brake()
            motor_r.brake()
            print("STOP")
            touch.wait_for_release()
            sleep(.5) # Drobný debouncing
            state = STATE_STOP
        else:
            print("START")
            # Reset PID regulátoru
            integral = 0 
            last_error = 0 
            touch.wait_for_release()
            sleep(.5) # Drobný debouncing
            state = STATE_RUNNING

    # Pokud jsme ve stavu RUNNING, PID regulátor se učí zjemňovat jízdu po černé
    # Cílem PIDu je to, aby robot zbrkle necukal sem a tam, ale měníl poarametry jízdy plynule, což je úkol správného nastavení složek P/I/D
    # a následného „učení“ regulátoru za jízdy
    if state == STATE_RUNNING:
        light = color.reflected_value()
        error = light - target
        integral += error
        integral = max(-1000, min(1000, integral))
        derivative = error - last_error
        turn = (Kp * error) + (Ki * integral) + (Kd * derivative)
        left_speed  = base + turn
        right_speed = base - turn
        left_speed  = max(-300, min(300, left_speed))
        right_speed = max(-300, min(300, right_speed))
        motor_l.run_at_speed(left_speed)
        motor_r.run_at_speed(right_speed)
        last_error = error
