# Minimal MCP23017 16-bit I2C GPIO expander driver for MicroPython.
#
# Pins are addressed 0-15: 0-7 = port A (GPA0-7), 8-15 = port B (GPB0-7).
# Uses the default IOCON.BANK=0 register layout, so the A/B register pairs
# are adjacent and can be accessed as little-endian 16-bit words.

# Register addresses (BANK = 0)
_IODIRA = 0x00   # IODIRA/IODIRB at 0x00/0x01
_GPPUA  = 0x0C   # GPPUA/GPPUB  at 0x0C/0x0D
_GPIOA  = 0x12   # GPIOA/GPIOB  at 0x12/0x13
_OLATA  = 0x14   # OLATA/OLATB  at 0x14/0x15

IN = 1
OUT = 0


class MCP23017:
    def __init__(self, i2c, address=0x20):
        self._i2c = i2c
        self._addr = address
        # Mirror registers so single-pin updates don't require a read first.
        self._iodir = 0xFFFF  # power-on default: all inputs
        self._gppu = 0x0000
        self._olat = 0x0000
        self._write16(_IODIRA, self._iodir)
        self._write16(_GPPUA, self._gppu)
        self._write16(_OLATA, self._olat)

    def _write16(self, reg, value):
        self._i2c.writeto_mem(
            self._addr, reg, bytes([value & 0xFF, (value >> 8) & 0xFF])
        )

    def _read16(self, reg):
        data = self._i2c.readfrom_mem(self._addr, reg, 2)
        return data[0] | (data[1] << 8)

    def pin_mode(self, pin, direction, pull_up=False):
        mask = 1 << pin
        if direction == IN:
            self._iodir |= mask
        else:
            self._iodir &= ~mask
        if pull_up:
            self._gppu |= mask
        else:
            self._gppu &= ~mask
        self._write16(_GPPUA, self._gppu)
        self._write16(_IODIRA, self._iodir)

    def output(self, pin, value):
        mask = 1 << pin
        if value:
            self._olat |= mask
        else:
            self._olat &= ~mask
        self._write16(_OLATA, self._olat)

    def input(self, pin):
        return (self._read16(_GPIOA) >> pin) & 1
