# Output pins
# Speed PWM stays on a native GPIO; the MCP23017 cannot generate a clean 20Hz
# signal.  Incline relays, incline buttons, and the safety key live on the
# MCP23017 expander (see below).
SPEED_PIN = 18

# Input pins
SPEED_POT_PIN = 34

# I2C display (SSD1306 128x64)
I2C_SDA = 21
I2C_SCL = 22

# MCP23017 I2C GPIO expander (incline relays + buttons + safety key)
# Dedicated I2C bus, separate from the SSD1306 display bus (21/22).
MCP_I2C_SDA = 16
MCP_I2C_SCL = 17
MCP_I2C_ADDR = 0x20      # A0-A2 tied to GND
MCP_INCLINE_UP = 0       # GPA0 -> relay: incline up
MCP_INCLINE_DOWN = 1     # GPA1 -> relay: incline down
MCP_INCLINE_UP_BTN = 8   # GPB0 <- active-low button (internal pull-up)
MCP_INCLINE_DOWN_BTN = 9 # GPB1 <- active-low button (internal pull-up)
MCP_SAFETY_KEY = None    # GPB2 <- safety key; set to 10 to enable, None to disable

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

# Rotary encoder
ENCODER_CLK = 32
ENCODER_DT = 33
ENCODER_SW = 35   # click button — active-low with internal pull-up

# Debug serial output
DEBUG = True
