# https://github.com/makerfaireczech/maker_badge/blob/main/SW/CircuitPython/examples/MF%20basic%20rev.D/code.py
import time
import terminalio
import board
import neopixel
import touchio
import displayio
import adafruit_ssd1680
from adafruit_display_text import label
from digitalio import DigitalInOut, Direction, Pull
import gc

# Print with free mem info
def printm(message):
    print(f"{gc.mem_free()} B\t{message}")

# Set layer/group visiblity
# parent -> child ->...
def set_layer_visibility(visible, parent, child):
    try:
        if visible == True:
            parent.append(child)
        else:
            parent.remove(child)
    except ValueError:
        pass

# Show layer and hide others in same parent
def show_layer(parent, layer):
    for _layer in parent:
        set_layer_visibility(False, parent, _layer)
    set_layer_visibility(True, parent, layer)

# Create label and return new group
def create_label(text, scale, color, x_cord, y_cord):
    group = displayio.Group(scale = scale, x = x_cord, y = y_cord)
    text_label = label.Label(terminalio.FONT, text = text, color = color)
    group.append(text_label)
    return group

printm("Zacatek programu")

# Define ePaper display colors value
display_black = 0x000000
display_white = 0xFFFFFF

# Define ePaper display resolution
display_width = 250
display_height = 122

display_background = displayio.Bitmap(display_width, display_height, 1)
display_color_palette = displayio.Palette(1)
display_color_palette[0] = display_white

# Define board pinout
board_spi = board.SPI()  # Uses SCK and MOSI
board_epd_cs = board.D41
board_epd_dc = board.D40
board_epd_reset = board.D39
board_epd_busy = board.D42
enable_display = DigitalInOut(board.D16)
enable_display.direction = Direction.OUTPUT

# Define touch buttons
touch_threshold = 20000
touch_1 = touchio.TouchIn(board.D5)
touch_1.threshold = touch_threshold
touch_2 = touchio.TouchIn(board.D4)
touch_2.threshold = touch_threshold
touch_3 = touchio.TouchIn(board.D3)
touch_3.threshold = touch_threshold
touch_4 = touchio.TouchIn(board.D2)
touch_4.threshold = touch_threshold
touch_5 = touchio.TouchIn(board.D1)
touch_5.threshold = touch_threshold

# Define LED
led_pin = board.D18
led_matrix = neopixel.NeoPixel(led_pin, 4, brightness = 0.1, auto_write = False)

# Define LED colors value
led_off = (0, 0, 0)
led_red = (255, 0, 0)
led_green = (0, 255, 0)
led_blue = (0, 0, 255)

# Prepare ePaper display
enable_display.value = False
displayio.release_displays()
display_bus = displayio.FourWire(
    board_spi, command = board_epd_dc, chip_select = board_epd_cs, reset = board_epd_reset, baudrate = 1000000
)
time.sleep(1)
display = adafruit_ssd1680.SSD1680(
    display_bus, 
    width = display_width, 
    height = display_height, 
    rotation = 270, 
    busy_pin = board_epd_busy,
    seconds_per_frame = .1
)

printm("Vytvarim vrstvy")

# Root layer
root = displayio.Group()
printm("Hlavni korenova vrstva vytvorena")

# 1. complex view
view_1 = displayio.Group()
view_1.append(displayio.TileGrid(display_background, pixel_shader = display_color_palette))
view_1.append(create_label("TOTO JE", 3, display_black, 5, 20))
view_1.append(create_label("PRVNI", 3, display_black, 5, 60))
view_1.append(create_label("OBRAZOVKA", 3, display_black, 5, 100))
printm("Prvni vrstva vytvorena")

# 2. complex view
view_2 = displayio.Group()
view_2.append(displayio.TileGrid(display_background, pixel_shader = display_color_palette))
view_2.append(create_label("TOTO JE", 3, display_black, 5, 20))
view_2.append(create_label("DRUHA", 3, display_black, 5, 60))
view_2.append(create_label("OBRAZOVKA", 3, display_black, 5, 100))
printm("Druha vrstva vytvorena")

# 3. complex view
view_3 = displayio.Group()
view_3.append(displayio.TileGrid(display_background, pixel_shader = display_color_palette))
view_3.append(create_label("TOTO JE", 3, display_black, 5, 20))
view_3.append(create_label("TRETI", 3, display_black, 5, 60))
view_3.append(create_label("OBRAZOVKA", 3, display_black, 5, 100))
printm("Treti vrstva vytvorena")

# 4. complex view
view_4 = displayio.Group()
view_4.append(displayio.TileGrid(display_background, pixel_shader = display_color_palette))
view_4.append(create_label("TOTO JE", 3, display_black, 5, 20))
view_4.append(create_label("CTVRTA", 3, display_black, 5, 60))
view_4.append(create_label("OBRAZOVKA", 3, display_black, 5, 100))
printm("Ctvrta vrstva vytvorena")

# 5. complex view
view_5 = displayio.Group()
view_5.append(displayio.TileGrid(display_background, pixel_shader = display_color_palette))
view_5.append(create_label("TOTO JE", 3, display_black, 5, 20))
view_5.append(create_label("PATA", 3, display_black, 5, 60))
view_5.append(create_label("OBRAZOVKA", 3, display_black, 5, 100))
printm("Pata vrstva vytvorena")

# Default view at start
show_layer(root, view_1)
display.show(root)
display.refresh()
printm("Prvni vrstva zobrazena")

# MAIN LOOP
while True:
    if touch_1.value:
        # Turn off the LED
        led_matrix.fill(led_off)
        led_matrix.show()
        show_layer(root, view_1) # Show 1. view
        display.show(root)
        display.refresh()
        printm("Prvni vrstva zobrazena")
    if touch_2.value:
        # Set LED to red
        led_matrix.fill(led_red)
        led_matrix.show()
        show_layer(root, view_2) # Show 2. view
        display.show(root)
        display.refresh()
        printm("Druha vrstva zobrazena")
    if touch_3.value:
        # Set LED to green
        led_matrix.fill(led_green)
        led_matrix.show()
        show_layer(root, view_3) # Show 3. view
        display.show(root)
        display.refresh()
        printm("Treti vrstva zobrazena")
    if touch_4.value:
        # Set LED to blue
        led_matrix.fill(led_blue)
        led_matrix.show()
        show_layer(root, view_4) # Show 4. view
        display.show(root)
        display.refresh()
        printm("Ctvrta vrstva zobrazena")
    if touch_5.value:
        # Turn off the LED
        led_matrix.fill(led_off)
        led_matrix.show() 
        show_layer(root, view_5) # Show 5. view
        display.show(root)
        display.refresh()
        printm("Pata vrstva zobrazena")
