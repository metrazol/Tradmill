# Output pins
SPEED_PIN = 18
INCLINE_UP_PIN = 19
INCLINE_DOWN_PIN = 23

# Input pins
SPEED_POT_PIN = 34
INCLINE_UP_BTN = 25
INCLINE_DOWN_BTN = 26
SAFETY_KEY_PIN = None  # Set to a pin number to enable, None to disable

# I2C display (SSD1306 128x64)
I2C_SDA = 21
I2C_SCL = 22

# Speed
PWM_FREQ = 20           # Hz — MC2100 expects 20Hz
MAX_DUTY_CYCLE = 0.55   # 55% duty = full speed
SPEED_ANALOG_THRESHOLD = 160  # ~40 scaled from Arduino 10-bit to ESP32 12-bit ADC
MAX_SPEED_MPH = 12.0

# Incline
MAX_INCLINE_LEVEL = 15
MIN_INCLINE_LEVEL = 0
INCLINE_BTN_ACTIVE_LOW = True  # True = buttons pull LOW when pressed (INPUT_PULLUP)

# Display refresh
DISPLAY_UPDATE_MS = 250
