/*	Geek Factory GFRTC Library
	Copyright (C) 2018 Jesus Ruben Santa Anna Zamudio.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	Author website: https://www.geekfactory.mx
	Author e-mail: ruben at geekfactory dot mx
 */
#include "GFRTC.h"

/*-------------------------------------------------------------*
 *		Class implementation				*
 *-------------------------------------------------------------*/
GFRTCClass::GFRTCClass()
{
	// Prepare I2C (moved this call out of the constructor) as it causes problems in some architectures
	//Wire.begin();
}

bool GFRTCClass::begin(bool begini2c)
{
	_isPresent = false;
	// request to initialize I2C?
	if (begini2c) {
		Wire.begin();
	}

	// check for presence by performing a read operation
	readRegister(0x00);

	return _isPresent;
}

timelib_t GFRTCClass::get()
{
	struct timelib_tm dt;

	// read information from RTC to structure
	if (read(dt) == false) {
		// return 0 if cannot communicate with RTC
		return 0;
	}

	// convert to timestamp
	return time_make(&dt);
}

bool GFRTCClass::set(timelib_t t)
{
	struct timelib_tm dt;

	// get human readable time information (as required by RTC chip)
	time_break(t, &dt);

	// enable CH bit for DS1307 chip
	dt.tm_sec |= 0x80;
	// write information on struct to hardware clock
	if (!write(dt))
		return false;
	// disable CH bit on DS1307
	dt.tm_sec &= 0x7f;
	if (!write(dt))
		return false;

	// return true if communication was successful
	return true;
}

bool GFRTCClass::read(struct timelib_tm &dt)
{
	uint8_t sec;
	_isPresent = false;

	Wire.beginTransmission(GFRTC_I2C_ADDRESS);
	// reset register pointer to seconds reg
	Wire.write((uint8_t) RTC_SECONDS);
	if (Wire.endTransmission() != 0) {
		return false;
	}

	// request the 7 data fields secs, min, hr, dow, date, mth, yr
	Wire.requestFrom(GFRTC_I2C_ADDRESS, 7);
	if (Wire.available() < 7) {
		return false;
	}

	// RTC responded as expected
	_isPresent = true;

	// read result and convert from BCD
	sec = Wire.read();
	dt.tm_sec = bcd2dec(sec & 0x7f);
	dt.tm_min = bcd2dec(Wire.read());
	dt.tm_hour = bcd2dec(Wire.read() & 0x3f); // mask assumes 24hr clock
	dt.tm_wday = bcd2dec(Wire.read());
	dt.tm_mday = bcd2dec(Wire.read());
	dt.tm_mon = bcd2dec(Wire.read());
	dt.tm_year = time_y2k2tm((bcd2dec(Wire.read())));

	// If clock is halted, return false
	if (sec & 0x80) {
		return false;
	}
	return true;
}

bool GFRTCClass::write(struct timelib_tm &dt)
{
	_isPresent = false;
	Wire.beginTransmission(GFRTC_I2C_ADDRESS);
	// reset register pointer to seconds reg
	Wire.write((uint8_t) RTC_SECONDS);
	// write date / time information
	Wire.write(dec2bcd(dt.tm_sec));
	Wire.write(dec2bcd(dt.tm_min));
	Wire.write(dec2bcd(dt.tm_hour));
	Wire.write(dec2bcd(dt.tm_wday));
	Wire.write(dec2bcd(dt.tm_mday));
	Wire.write(dec2bcd(dt.tm_mon));
	Wire.write(dec2bcd(time_tm2y2k(dt.tm_year)));
	if (Wire.endTransmission() != 0) {
		return false;
	}
	// communication successful
	_isPresent = true;
	return true;
}

uint8_t GFRTCClass::readRegister(uint8_t addr, bool * result)
{
	uint8_t reg;

	bool ret = readRegister(addr, &reg, sizeof(reg));

	if (result != NULL)
		* result = ret;

	return reg;
}

bool GFRTCClass::readRegister(uint8_t addr, void * data, uint8_t size)
{
	uint8_t i;
	uint8_t * dst = (uint8_t *) data;

	// limitation of wire library
	if (size > 32)
		return false;

	_isPresent = false;
	// begin operation on I2C
	Wire.beginTransmission(GFRTC_I2C_ADDRESS);
	Wire.write(addr);

	// prepare to read
	if (Wire.endTransmission() != 0) {
		return false;
	}

	// begin read operation
	Wire.requestFrom(GFRTC_I2C_ADDRESS, size);
	if (Wire.available() < size) {
		return false;
	}
	// Read data to buffer
	for (i = 0; i < size; i++) {
		dst[i] = Wire.read();
	}

	_isPresent = true;
	return true;
}

bool GFRTCClass::writeRegister(uint8_t addr, uint8_t value)
{
	return writeRegister(addr, &value, sizeof(value));
}

bool GFRTCClass::writeRegister(uint8_t addr, const void * data, uint8_t size)
{
	uint8_t i;
	uint8_t * src = (uint8_t *) data;

	// limitation of wire library
	if (size > 32)
		return false;

	_isPresent = false;
	// begin operation on I2C
	Wire.beginTransmission(GFRTC_I2C_ADDRESS);
	Wire.write(addr);

	// write desired data
	for (i = 0; i < size; i++) {
		Wire.write(src[i]);
	}

	// check if communication was successful
	if (Wire.endTransmission() != 0) {
		return false;
	} else {
		_isPresent = true;
		return true;
	}
}

bool GFRTCClass::readNVRAM(uint16_t address, void * buffer, uint16_t size)
{
	return true;
}

bool GFRTCClass::writeNVRAM(uint16_t address, const void * buffer, uint16_t size)
{
	return true;
}

bool GFRTCClass::setAlarm(gfrtc_alarm_types type, uint8_t hour, uint8_t minute, uint8_t second, uint8_t dow)
{
	uint8_t addr;

	second = dec2bcd(second);
	minute = dec2bcd(minute);
	hour = dec2bcd(hour);
	dow = dec2bcd(dow);

	if (type & 0x01) second |= 1 << A1M1;
	if (type & 0x02) minute |= 1 << A1M2;
	if (type & 0x04) hour |= 1 << A1M3;
	if (type & 0x10) dow |= 1 << DYDT;
	if (type & 0x08) dow |= 1 << A1M4;

	if (!(type & 0x80)) // alarm 1
	{
		addr = ALM1_SECONDS;
		if (!writeRegister(addr++, second)) {
			return false;
		}
	} else {
		addr = ALM2_MINUTES;
	}
	if (!writeRegister(addr++, minute) || !writeRegister(addr++, hour) || !writeRegister(addr++, dow)) {
		return false;
	}
	return true;
}

bool GFRTCClass::setAlarmInterrupt(enum gfrtc_alarms alarm, bool enable)
{
	uint8_t regval, mask;
	bool res;

	// read control register value
	regval = readRegister(RTC_CONTROL, & res);
	if (!res) {
		return false;
	}

	// prepare bitmask according to requested interrupt
	switch (alarm) {
	case E_ALARM_1:
		mask = 1 << A1IE;
		break;
	case E_ALARM_2:
		mask = 1 << A2IE;
		break;
	default:
		return false;
	}

	// enable or disable interrupt bit
	if (enable)
		regval |= mask;
	else
		regval &= ~mask;

	if (!writeRegister(RTC_CONTROL, regval))
		return false;

	return true;
}

bool GFRTCClass::getAlarmInterruptFlag(enum gfrtc_alarms alarm)
{
	uint8_t regval, mask;
	bool res;

	// read status register value
	regval = readRegister(RTC_STATUS, &res);
	if (!res) {
		return false;
	}

	// prepare bitmask according to requested interrupt
	switch (alarm) {
	case E_ALARM_1:
		mask = 1 << A1F;
		break;
	case E_ALARM_2:
		mask = 1 << A2F;
		break;
	default:
		return false;
	}

	// check for interrupt flag bits
	if (regval & mask) {
		regval &= ~mask;
		writeRegister(RTC_STATUS, regval);
		return true;
	} else {
		return false;
	}
}

bool GFRTCClass::setSquareWave(enum gfrtc_wave_frequencies frequency)
{
	uint8_t controlReg;

	controlReg = readRegister(RTC_CONTROL);
	if (freq >= SQWAVE_NONE) {
		controlReg |= _BV(INTCN);
	} else {
		controlReg = (controlReg & 0xE3) | (freq << RS1);
	}
	writeRegister(RTC_CONTROL, controlReg);
}

bool GFRTCClass::getOscStoppedFlag(bool clearosf)
{
	// read status register
	uint8_t s = readRegister(RTC_STATUS);
	bool ret = s & (1 << OSF);
	if (ret && clearosf) {
		writeRegister(RTC_STATUS, s & ~(1 << OSF));
	}
	return ret;
}

int16_t GFRTCClass::getTemperature()
{
	// union to convert bytes to int

	union int16_byte {
		int16_t i;
		byte b[2];
	} rtcTemp;

	rtcTemp.b[0] = readRegister(RTC_TEMP_LSB);
	rtcTemp.b[1] = readRegister(RTC_TEMP_MSB);
	return rtcTemp.i / 64;
}

bool GFRTCClass::isPresent()
{
	return _isPresent;
}

uint8_t GFRTCClass::dec2bcd(uint8_t num)
{
	return((num / 10 * 16) + (num % 10));
}

uint8_t GFRTCClass::bcd2dec(uint8_t num)
{
	return((num / 16 * 10) + (num % 16));
}

bool GFRTCClass::_isPresent = false;

/**
 * Create an instance for the user
 */
GFRTCClass GFRTC = GFRTCClass();
