import brian.sensors as sensors # Knihovna pro ovládání čidel
import brian.motors as motors # Knihovna pro ovládání motorů
from time import sleep

# Inicializace čidel
print("Inicializuji senzory... ", end="")
ultrasonic = sensors.EV3.UltrasonicSensorEV3(sensors.SensorPort.S1)
ultrasonic.wait_until_ready()
touch = sensors.EV3.TouchSensorEV3(sensors.SensorPort.S2)
touch.wait_until_ready()
print("OK")

# Inicializace předních motorů L/R (levý/pravý) a kladiva (hammer)
print("Inicializuji motory... ", end="")
motor_l = motors.EV3LargeMotor(motors.MotorPort.A)
motor_l.wait_until_ready()
motor_r = motors.EV3LargeMotor(motors.MotorPort.B)
motor_r.wait_until_ready()
hammer = motors.EV3MediumMotor(motors.MotorPort.C)
hammer.wait_until_ready()
print("OK")

# Stavy programu, které nastavujeme stiskem tlačítka na zádi
STATE_RUNNING = 1
STATE_STOP = 0
state = STATE_STOP

# Funkce pro dynamický návrat kladiva do výchozí pozice
# Motor kladiva je relativně slabý, a proto používáme gumičku, kterou motor napne a ta pak pomůže zvednout kladivo
# Motor nejprve na určitý počet sekund (implus_sec) roztočíme rychlostí (speed)
# Poté začneme měřit, jestli už není v dorazu, a proto se v zátěži dále netočí (is_stalled)
# Stav is_stalled schváleně neměříme hned od začátku, protože slabý motor v něm může být i při zvedání těžkého kladiva; proto mu pomáhá napnutá gumička
def hammerReturn(impuls_sec, speed):
    hammer.run_at_speed(speed)
    sleep(impuls_sec)
    while hammer.is_stalled == False:
        hammer.run_at_speed(speed)
        sleep(0.05)
    hammer.brake()

# Funkce pro shození kladiva vlastní vahou vpřed
# Vyvoláme pouze krátný iniciační impulz z dorazové pozice 
def hammerFire(impuls_sec, speed):
    hammer.run_at_speed(-1 * speed)
    sleep(impuls_sec)
    hammer.brake()

# Čekáme na stisk tlačítka pro start, poté roztočíme motory
print("Nastartuj tlacitkem")

# Nekonečná smyčka
while True:
    # Pokud jsme stiskli tlačítko, podle aktuálního stavu zastavíme, nebo opět roztočíme motory
    # Pozor, tlačítko je pro jednoduchost synchronní, takže jeho stav čteme pouze zde a nebude reagovat, 
    # když bude program provádět práci po detekci překážky 
    if touch.is_pressed():
        if state == STATE_RUNNING:
            motor_l.brake()
            motor_r.brake()
            hammer.brake()
            print("STOP")
            touch.wait_for_release()
            sleep(.5) # Drobný debouncing
            state = STATE_STOP
        else:
            print("START")
            touch.wait_for_release()
            sleep(.5) # Drobný debouncing
            state = STATE_RUNNING
            motor_l.run_at_speed(300)
            motor_r.run_at_speed(300)

    # Pokud jsme ve stavu RUNNING, změříme vzdálenost k překářce před robotem
    # Pokud bude menší než 75 mm, spustíme funkce pro práci s kladivem
    # Poté trošku couvneme, pootočíme se a pokračujeme v jízdě
    if state == STATE_RUNNING:
        if ultrasonic.distance_mm() < 75:
            motor_l.brake()
            motor_r.brake()
            sleep(1)
            hammerFire(.75, 1000)
            sleep(1)
            hammerReturn(2, 1000)
            sleep(1)
            motor_l.rotate_by_angle(-360, 300, 0)
            motor_r.rotate_by_angle(-360, 300)
            motor_l.rotate_by_angle(200, 300, 0)
            motor_r.rotate_by_angle(-200, 300)
            motor_l.run_at_speed(300)
            motor_r.run_at_speed(300)
