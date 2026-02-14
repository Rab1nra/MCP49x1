/**
 * @file MCP49x1.cpp
 * @brief Implementation of the MCP49x1 library for ESP32 / Arduino Core.
 *
 * Datasheet reference: Microchip DS22248A
 *
 * ──────────────────────────────────────────────────────
 *  16-bit Write Command Register (all three devices):
 *
 *  Bit 15 : Always 0  (write to DAC register)
 *  Bit 14 : BUF       (VREF buffer: 1=buffered, 0=unbuffered)
 *  Bit 13 : /GA       (Gain: 1=1x, 0=2x)
 *  Bit 12 : /SHDN     (1=active, 0=shutdown)
 *  Bits 11-0 for MCP4921: D11..D0  (12-bit data, MSB-first)
 *  Bits 11-2 for MCP4911: D9..D0   (10-bit data, left-aligned into bits 11-2)
 *  Bits 11-4 for MCP4901: D7..D0   (8-bit  data, left-aligned into bits 11-4)
 * ──────────────────────────────────────────────────────
 */

#include "MCP49x1.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal bit-field constants
// ─────────────────────────────────────────────────────────────────────────────
#define MCP_BIT_WRITE  (0 << 15)   // bit 15 always 0
#define MCP_BIT_BUF    (1 << 14)   // VREF buffered
#define MCP_BIT_GA1X   (1 << 13)   // Gain = 1x  (/GA = 1)
#define MCP_BIT_GA2X   (0 << 13)   // Gain = 2x  (/GA = 0)
#define MCP_BIT_ACTIVE (1 << 12)   // SHDN = 1  → active output
#define MCP_BIT_SHDN   (0 << 12)   // SHDN = 0  → shutdown


// ─────────────────────────────────────────────────────────────────────────────
//  Hardware SPI constructor
// ─────────────────────────────────────────────────────────────────────────────
MCP49x1::MCP49x1(MCP49x1_Model model,
                 int8_t        csPin,
                 int8_t        ldacPin,
                 float         vref)
    : _model(model),
      _gain(GAIN_1X),
      _vrefMode(VREF_UNBUFFERED),
      _spiMode(MCP_SPI_HW),
      _csPin(csPin),
      _ldacPin(ldacPin),
      _sckPin(-1),
      _mosiPin(-1),
      _vref(vref),
      _lastCode(0),
      _isShutdown(false),
      _autoLatch(true),
      _begun(false)
{
    _bits    = (uint8_t)model;
    _maxCode = (uint16_t)((1u << _bits) - 1u);
    _spiFreq = 8000000UL;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Software SPI constructor
// ─────────────────────────────────────────────────────────────────────────────
MCP49x1::MCP49x1(MCP49x1_Model model,
                 int8_t        csPin,
                 int8_t        sckPin,
                 int8_t        mosiPin,
                 int8_t        ldacPin,
                 float         vref)
    : _model(model),
      _gain(GAIN_1X),
      _vrefMode(VREF_UNBUFFERED),
      _spiMode(MCP_SPI_SW),
      _csPin(csPin),
      _ldacPin(ldacPin),
      _sckPin(sckPin),
      _mosiPin(mosiPin),
      _vref(vref),
      _lastCode(0),
      _isShutdown(false),
      _autoLatch(true),
      _begun(false)
{
    _bits    = (uint8_t)model;
    _maxCode = (uint16_t)((1u << _bits) - 1u);
    _spiFreq = 0; // unused for SW SPI
}

// ─────────────────────────────────────────────────────────────────────────────
//  begin()
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::begin(uint32_t spiFreq)
{
    if (_begun) return;

    _spiFreq = spiFreq;

    // Chip-Select: start HIGH (deselected)
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);

    // LDAC pin (optional)
    if (_ldacPin != MCP49X1_LDAC_TIED_LOW) {
        pinMode(_ldacPin, OUTPUT);
        digitalWrite(_ldacPin, HIGH); // idle HIGH
    }

    // SPI bus init
    if (_spiMode == MCP_SPI_HW) {
        SPI.begin();
        SPI.setFrequency(_spiFreq);
        SPI.setDataMode(SPI_MODE0);   // CPOL=0, CPHA=0 → Mode 0,0
        SPI.setBitOrder(MSBFIRST);
    } else {
        // Software SPI GPIO init
        pinMode(_sckPin,  OUTPUT);
        pinMode(_mosiPin, OUTPUT);
        digitalWrite(_sckPin,  LOW);
        digitalWrite(_mosiPin, LOW);
    }

    _begun = true;

    // Send an initial write at code 0 to bring the device out of POR state
    // (datasheet Section 4.2.3: device stays high-Z until first valid write)
    setRaw(0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  end()
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::end()
{
    if (!_begun) return;
    shutdown();
    if (_spiMode == MCP_SPI_HW) {
        SPI.end();
    }
    _begun = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  setGain()
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::setGain(MCP49x1_Gain gain)
{
    _gain = gain;
    // Re-send last code so gain takes effect immediately
    if (_begun && !_isShutdown) {
        setRaw(_lastCode);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  setVRefMode()
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::setVRefMode(MCP49x1_VRefMode mode)
{
    _vrefMode = mode;
    if (_begun && !_isShutdown) {
        setRaw(_lastCode);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  setVRef()
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::setVRef(float vref)
{
    _vref = vref;
    // No SPI write needed — purely affects software voltage calculations
}

// ─────────────────────────────────────────────────────────────────────────────
//  setRaw()
// ─────────────────────────────────────────────────────────────────────────────
int MCP49x1::setRaw(uint16_t code)
{
    int status = MCP49x1_OK;

    // Clamp to valid range
    if (code > _maxCode) {
        code   = _maxCode;
        status = MCP49x1_ERR_CLAMP;
    }

    _lastCode    = code;
    _isShutdown  = false;

    uint16_t cmd = _buildCommand(code, true /* active */);
    _writeCommand(cmd);
    _doLatch();

    return status;
}

// ─────────────────────────────────────────────────────────────────────────────
//  setPercent()
// ─────────────────────────────────────────────────────────────────────────────
int MCP49x1::setPercent(float fraction)
{
    int status = MCP49x1_OK;

    if (fraction < 0.0f) {
        fraction = 0.0f;
        status   = MCP49x1_ERR_CLAMP;
    } else if (fraction > 1.0f) {
        fraction = 1.0f;
        status   = MCP49x1_ERR_CLAMP;
    }

    uint16_t code = (uint16_t)roundf(fraction * (float)_maxCode);
    int raw_status = setRaw(code);

    // Preserve the more informative status
    return (status != MCP49x1_OK) ? status : raw_status;
}

// ─────────────────────────────────────────────────────────────────────────────
//  setVoltage()
//
//  VOUT = VREF × (D / 2^n) × Gain   (datasheet Eq. 4-1)
//  → D  = round(VOUT / (VREF × Gain) × 2^n)
//         = round(VOUT / getMaxVoltage() × _maxCode)
// ─────────────────────────────────────────────────────────────────────────────
int MCP49x1::setVoltage(float voltage)
{
    int status = MCP49x1_OK;

    float maxV = getMaxVoltage();

    if (voltage < 0.0f) {
        voltage = 0.0f;
        status  = MCP49x1_ERR_CLAMP;
    } else if (voltage > maxV) {
        voltage = maxV;
        status  = MCP49x1_ERR_CLAMP;
    }

    uint16_t code = (uint16_t)roundf((voltage / maxV) * (float)_maxCode);
    int raw_status = setRaw(code);

    return (status != MCP49x1_OK) ? status : raw_status;
}

// ─────────────────────────────────────────────────────────────────────────────
//  shutdown()
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::shutdown()
{
    if (!_begun) return;

    // Send command with SHDN=0 to enter shutdown
    uint16_t cmd = _buildCommand(0, false /* shutdown */);
    _writeCommand(cmd);
    _doLatch();

    _isShutdown = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  wakeup()
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::wakeup()
{
    if (!_begun) return;
    _isShutdown = false;
    // Re-send last code with SHDN=1
    uint16_t cmd = _buildCommand(_lastCode, true);
    _writeCommand(cmd);
    _doLatch();
}

// ─────────────────────────────────────────────────────────────────────────────
//  latchOutput()  — manual LDAC pulse
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::latchOutput()
{
    if (_ldacPin == MCP49X1_LDAC_TIED_LOW) return;
    // Datasheet: tLD (LDAC pulse width) minimum 100 ns
    digitalWrite(_ldacPin, LOW);
    delayMicroseconds(1); // well above 100 ns minimum
    digitalWrite(_ldacPin, HIGH);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostic helpers
// ─────────────────────────────────────────────────────────────────────────────
float MCP49x1::getMaxVoltage() const
{
    return _vref * (float)_gain;
}

float MCP49x1::getLSB() const
{
    // LSB = VREF × Gain / 2^n  (see datasheet Table 4-1)
    return (_vref * (float)_gain) / (float)(1u << _bits);
}

float MCP49x1::getLastVoltage() const
{
    // VOUT = VREF × (D / 2^n) × Gain
    return (_vref * (float)_gain * (float)_lastCode) / (float)(1u << _bits);
}

void MCP49x1::printConfig() const
{
    Serial.println(F("─── MCP49x1 Configuration ───────────────"));
    Serial.print(F("  Model      : MCP49"));
    Serial.println(_bits == 8 ? F("01 (8-bit)")
                              : _bits == 10 ? F("11 (10-bit)")
                                            : F("21 (12-bit)"));
    Serial.print(F("  Resolution : "));
    Serial.print(_bits);
    Serial.println(F(" bits"));
    Serial.print(F("  Max code   : "));
    Serial.println(_maxCode);
    Serial.print(F("  VREF       : "));
    Serial.print(_vref, 4);
    Serial.println(F(" V"));
    Serial.print(F("  Gain       : "));
    Serial.println(_gain == GAIN_1X ? F("1x") : F("2x"));
    Serial.print(F("  VREF mode  : "));
    Serial.println(_vrefMode == VREF_UNBUFFERED ? F("Unbuffered") : F("Buffered"));
    Serial.print(F("  Max VOUT   : "));
    Serial.print(getMaxVoltage(), 4);
    Serial.println(F(" V"));
    Serial.print(F("  LSB size   : "));
    Serial.print(getLSB() * 1000.0f, 4);
    Serial.println(F(" mV"));
    Serial.print(F("  SPI mode   : "));
    Serial.println(_spiMode == MCP_SPI_HW ? F("Hardware") : F("Software (bit-bang)"));
    Serial.print(F("  Shutdown   : "));
    Serial.println(_isShutdown ? F("YES") : F("No"));
    Serial.print(F("  Auto-latch : "));
    Serial.println(_autoLatch ? F("Yes") : F("No"));
    Serial.print(F("  Last code  : "));
    Serial.println(_lastCode);
    Serial.print(F("  Last VOUT  : "));
    Serial.print(getLastVoltage(), 4);
    Serial.println(F(" V"));
    Serial.println(F("─────────────────────────────────────────"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  _buildCommand()
//
//  Constructs the 16-bit SPI word per datasheet Registers 5-1 / 5-2 / 5-3.
//
//  MCP4921 (12-bit): data occupies bits 11-0 directly.
//  MCP4911 (10-bit): data must be left-shifted by 2 (bits 11-2).
//  MCP4901  (8-bit): data must be left-shifted by 4 (bits 11-4).
// ─────────────────────────────────────────────────────────────────────────────
uint16_t MCP49x1::_buildCommand(uint16_t code, bool active)
{
    uint16_t cmd = MCP_BIT_WRITE;

    // BUF bit
    if (_vrefMode == VREF_BUFFERED) {
        cmd |= MCP_BIT_BUF;
    }

    // /GA bit  (GA=1 → 1x gain,  GA=0 → 2x gain)
    if (_gain == GAIN_1X) {
        cmd |= MCP_BIT_GA1X;
    } else {
        cmd |= MCP_BIT_GA2X;
    }

    // /SHDN bit
    if (active) {
        cmd |= MCP_BIT_ACTIVE;
    } else {
        cmd |= MCP_BIT_SHDN;
    }

    // Data bits — align to bits 11-0 of the 12-bit data field
    uint16_t shiftedCode;
    switch (_bits) {
        case 12:
            // MCP4921: D11..D0 in bits 11-0 — no shift needed
            shiftedCode = code & 0x0FFF;
            break;
        case 10:
            // MCP4911: D9..D0 must occupy bits 11-2 (left-shift by 2)
            shiftedCode = (code & 0x03FF) << 2;
            break;
        case 8:
        default:
            // MCP4901: D7..D0 must occupy bits 11-4 (left-shift by 4)
            shiftedCode = (code & 0x00FF) << 4;
            break;
    }

    cmd |= shiftedCode;
    return cmd;
}

// ─────────────────────────────────────────────────────────────────────────────
//  _writeCommand()
//  Drives CS low, transfers 16 bits (MSB-first), then CS high.
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::_writeCommand(uint16_t command)
{
    if (_spiMode == MCP_SPI_HW) {
        // Acquire bus, ensure correct settings (other libs may change these)
        SPI.beginTransaction(SPISettings(_spiFreq, MSBFIRST, SPI_MODE0));
        digitalWrite(_csPin, LOW);
        SPI.transfer16(command);
        digitalWrite(_csPin, HIGH);
        SPI.endTransaction();
    } else {
        digitalWrite(_csPin, LOW);
        _swSPITransfer16(command);
        digitalWrite(_csPin, HIGH);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  _swSPITransfer16()  — bit-bang 16 bits MSB-first, SPI Mode 0,0
//  SCK idles LOW, data sampled on rising edge (CPOL=0, CPHA=0).
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::_swSPITransfer16(uint16_t data)
{
    for (int8_t bit = 15; bit >= 0; bit--) {
        // Set MOSI before clock rises
        digitalWrite(_mosiPin, (data >> bit) & 0x01 ? HIGH : LOW);
        // tSU (setup) minimum 15 ns — GPIO call is plenty
        digitalWrite(_sckPin, HIGH);
        // tHD (hold) minimum 10 ns
        digitalWrite(_sckPin, LOW);
    }
    digitalWrite(_mosiPin, LOW); // leave data line low
}

// ─────────────────────────────────────────────────────────────────────────────
//  _doLatch()  — pulse LDAC if auto-latch is enabled
// ─────────────────────────────────────────────────────────────────────────────
void MCP49x1::_doLatch()
{
    if (_autoLatch) {
        latchOutput();
    }
}
