import time

try:
    import board
    import busio
    import adafruit_ads1x15.ads1115 as ADS
    from adafruit_ads1x15.analog_in import AnalogIn
    _has_ads = True
except ImportError:
    _has_ads = False

from config import ADS1115_ADDR, SPEED_ADC_CHANNEL, INCLINE_ADC_CHANNEL, ADC_RAW_MIN, ADC_RAW_MAX

try:
    from smbus2 import SMBus
    _has_smbus = True
except ImportError:
    _has_smbus = False

_raw_max_counts = 26400
_gain_lsb = 0.125


class ADCReader:
    def __init__(self, bus_num=1, address=ADS1115_ADDR):
        self.ok = False
        self._ads = None
        self._ch_speed = None
        self._ch_incline = None
        self._mock_speed = 0
        self._mock_incline = 0
        self._last_speed = 0
        self._last_incline = 0
        if _has_ads:
            try:
                i2c = busio.I2C(board.SCL, board.SDA)
                self._ads = ADS.ADS1115(i2c, address=address)
                self._ads.gain = 1
                self._ads.data_rate = 860
                mapping = [ADS.P0, ADS.P1, ADS.P2, ADS.P3]
                self._ch_speed = AnalogIn(self._ads, mapping[SPEED_ADC_CHANNEL])
                self._ch_incline = AnalogIn(self._ads, mapping[INCLINE_ADC_CHANNEL])
                self.ok = True
                return
            except Exception:
                pass
        if _has_smbus:
            try:
                b = SMBus(bus_num)
                b.read_byte_data(address, 0x00)
                b.close()
                self.ok = False
            except OSError:
                pass

    def _smbus_single(self, channel):
        try:
            bus = SMBus(1)
            config = 0x8583 | (channel << 12) | (1 << 15)
            config |= (1 << 9)
            hi = (config >> 8) & 0xFF
            lo = config & 0xFF
            bus.write_i2c_block_data(ADS1115_ADDR, 0x01, [hi, lo])
            time.sleep(0.002)
            data = bus.read_i2c_block_data(ADS1115_ADDR, 0x00, 2)
            bus.close()
            raw = (data[0] << 8) | data[1]
            if raw > 32767:
                raw -= 65536
            if raw < 0:
                raw = 0
            scaled = int(raw * _gain_lsb / 0.125)
            return max(0, min(scaled, 32767))
        except OSError:
            return 0

    def _to_counts(self, raw16):
        if raw16 < 0:
            raw16 = 0
        mv = raw16 * _gain_lsb
        counts = int(mv / 0.125)
        return max(0, min(counts, 32767))

    def read_raw(self, channel):
        if self._ch_speed is not None:
            try:
                if channel == SPEED_ADC_CHANNEL:
                    return int(self._ch_speed.value)
                else:
                    return int(self._ch_incline.value)
            except Exception:
                return 0
        if _has_smbus:
            return self._smbus_single(channel)
        return 0

    def read_speed_raw(self):
        v = self.read_raw(SPEED_ADC_CHANNEL)
        self._last_speed = v
        return v

    def read_incline_raw(self):
        v = self.read_raw(INCLINE_ADC_CHANNEL)
        self._last_incline = v
        return v

    def read_speed_norm(self):
        raw = self.read_speed_raw()
        span = ADC_RAW_MAX - ADC_RAW_MIN
        if span <= 0:
            return 0.0
        n = (raw - ADC_RAW_MIN) / span
        return max(0.0, min(1.0, n))

    def read_incline_norm(self):
        raw = self.read_incline_raw()
        span = ADC_RAW_MAX - ADC_RAW_MIN
        if span <= 0:
            return 0.0
        n = (raw - ADC_RAW_MIN) / span
        return max(0.0, min(1.0, n))

    def set_mock(self, speed_raw, incline_raw):
        self._mock_speed = speed_raw
        self._mock_incline = incline_raw

    def close(self):
        pass
