try:
    from smbus2 import SMBus
    _has_smbus = True
except ImportError:
    _has_smbus = False

_IODIRA = 0x00
_GPPUA = 0x0C
_GPIOA = 0x12
_OLATA = 0x14

IN = 1
OUT = 0


class MCP23017:
    def __init__(self, bus=1, address=0x20):
        self.bus_num = bus
        self.address = address
        self._iodir = 0xFFFF
        self._gppu = 0x0000
        self._olat = 0x0000
        self.ok = False
        self._bus = None
        if not _has_smbus:
            return
        try:
            self._bus = SMBus(bus)
            self._write16(_IODIRA, self._iodir)
            self._write16(_GPPUA, self._gppu)
            self._write16(_OLATA, self._olat)
            self.ok = True
        except OSError:
            self.ok = False

    def _write16(self, reg, value):
        if self._bus is None:
            return
        lo = value & 0xFF
        hi = (value >> 8) & 0xFF
        self._bus.write_i2c_block_data(self.address, reg, [lo, hi])

    def _read16(self, reg):
        if self._bus is None:
            return 0
        data = self._bus.read_i2c_block_data(self.address, reg, 2)
        return data[0] | (data[1] << 8)

    def pin_mode(self, pin, direction, pull_up=False):
        if pin == -1:
            return
        mask = 1 << pin
        if direction == IN:
            self._iodir |= mask
        else:
            self._iodir &= ~mask
        if pull_up:
            self._gppu |= mask
        else:
            self._gppu &= ~mask
        try:
            self._write16(_GPPUA, self._gppu)
            self._write16(_IODIRA, self._iodir)
        except OSError:
            self.ok = False

    def output(self, pin, value):
        if pin == -1:
            return
        mask = 1 << pin
        if value:
            self._olat |= mask
        else:
            self._olat &= ~mask
        try:
            self._write16(_OLATA, self._olat)
        except OSError:
            self.ok = False

    def input(self, pin):
        if pin == -1 or self._bus is None:
            return 1
        try:
            return (self._read16(_GPIOA) >> pin) & 1
        except OSError:
            self.ok = False
            return 1

    def close(self):
        if self._bus is not None:
            try:
                self._bus.close()
            except OSError:
                pass
