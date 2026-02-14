/**
 * @file MCP49x1.h
 * @brief Full-featured Arduino/ESP32 library for Microchip MCP4901/MCP4911/MCP4921 DACs
 *
 * Supports:
 *   - MCP4901 :  8-bit DAC, external VREF
 *   - MCP4911 : 10-bit DAC, external VREF
 *   - MCP4921 : 12-bit DAC, external VREF
 *
 * Features:
 *   - Hardware SPI (fast) and Software SPI (any GPIO pins)
 *   - Buffered / Unbuffered VREF mode
 *   - 1x / 2x gain selection
 *   - Software Shutdown / Wake-up
 *   - LDAC pin support (synchronous multi-DAC latching)
 *   - Voltage-based output    → setVoltage(float volts)
 *   - Percentage-based output → setPercent(float 0.0–1.0)
 *   - Raw code output         → setRaw(uint16_t code)
 *   - Multiple DACs on the same SPI bus
 *
 * Datasheet: Microchip DS22248A
 * Default ESP32 VSPI pins: SCK=18, MOSI=23, CS=5
 *
 * @version 1.0.0
 */

#ifndef MCP49X1_H
#define MCP49X1_H

#include <Arduino.h>
#include <SPI.h>

// ─────────────────────────────────────────────────────────────────────────────
// Device model
// ─────────────────────────────────────────────────────────────────────────────
/**
 * @brief Supported DAC device variants.
 * The value equals the DAC resolution in bits.
 */
enum MCP49x1_Model {
    MCP4901 = 8,   ///< 8-bit  DAC  (codes 0 – 255)
    MCP4911 = 10,  ///< 10-bit DAC  (codes 0 – 1023)
    MCP4921 = 12   ///< 12-bit DAC  (codes 0 – 4095)
};

// ─────────────────────────────────────────────────────────────────────────────
// Gain selection  (datasheet Section 4.2.1.1)
// ─────────────────────────────────────────────────────────────────────────────
/**
 * @brief Output amplifier gain.
 *
 * GAIN_1X : GA bit = 1 → VOUT = VREF × D / 2^n
 * GAIN_2X : GA bit = 0 → VOUT = 2 × VREF × D / 2^n
 *
 * Default power-on value is GAIN_2X (GA bit resets to 0).
 */
enum MCP49x1_Gain {
    GAIN_1X = 1,
    GAIN_2X = 2
};

// ─────────────────────────────────────────────────────────────────────────────
// VREF buffer mode  (datasheet Section 4.2.2)
// ─────────────────────────────────────────────────────────────────────────────
/**
 * @brief VREF input amplifier configuration.
 *
 * UNBUFFERED : BUF bit = 0 → 0V–VDD, ~165 kΩ input impedance (default)
 * BUFFERED   : BUF bit = 1 → 40 mV – (VDD-40 mV), very high impedance
 */
enum MCP49x1_VRefMode {
    VREF_UNBUFFERED = 0,
    VREF_BUFFERED   = 1
};

// ─────────────────────────────────────────────────────────────────────────────
// SPI bus mode
// ─────────────────────────────────────────────────────────────────────────────
enum MCP49x1_SPIMode {
    MCP_SPI_HW,   ///< Hardware SPI peripheral (uses Arduino SPI library)
    MCP_SPI_SW    ///< Software (bit-bang) SPI — any GPIO pins
};

// ─────────────────────────────────────────────────────────────────────────────
// Return / status codes
// ─────────────────────────────────────────────────────────────────────────────
enum MCP49x1_Status {
    MCP49x1_OK           =  0,  ///< Success
    MCP49x1_ERR_CLAMP    = -1,  ///< Input was out of range and has been clamped
    MCP49x1_ERR_PARAM    = -2,  ///< Invalid parameter
    MCP49x1_ERR_SHUTDOWN = -3   ///< Device is in shutdown; write was blocked
};

// ─────────────────────────────────────────────────────────────────────────────
// Sentinel for "LDAC tied permanently to GND"
// ─────────────────────────────────────────────────────────────────────────────
#define MCP49X1_LDAC_TIED_LOW  (-1)

// ─────────────────────────────────────────────────────────────────────────────
//  MCP49x1  —  Main class
// ─────────────────────────────────────────────────────────────────────────────
class MCP49x1 {
public:

    // ──────────────────────────────────────────────────────────────────────────
    // Constructors
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * @brief Hardware SPI constructor.
     *
     * Example (ESP32 default VSPI):
     *   MCP49x1 dac(MCP4921, 5);             // CS=5, LDAC tied to GND
     *   MCP49x1 dac(MCP4921, 5, 15, 3.3f);   // CS=5, LDAC=15, VREF=3.3V
     *
     * @param model    MCP4901 / MCP4911 / MCP4921
     * @param csPin    Chip-Select GPIO (active LOW)
     * @param ldacPin  LDAC GPIO, or MCP49X1_LDAC_TIED_LOW (default)
     * @param vref     External reference voltage in volts (default 3.3 V)
     */
    MCP49x1(MCP49x1_Model model,
            int8_t        csPin,
            int8_t        ldacPin = MCP49X1_LDAC_TIED_LOW,
            float         vref    = 3.3f);

    /**
     * @brief Software (bit-bang) SPI constructor.
     *
     * Example:
     *   MCP49x1 dac(MCP4921, 5, 18, 23);   // CS=5, SCK=18, MOSI=23
     *
     * @param model    MCP4901 / MCP4911 / MCP4921
     * @param csPin    Chip-Select GPIO (active LOW)
     * @param sckPin   Clock GPIO
     * @param mosiPin  Data GPIO (SDI on the chip)
     * @param ldacPin  LDAC GPIO, or MCP49X1_LDAC_TIED_LOW (default)
     * @param vref     External reference voltage in volts (default 3.3 V)
     */
    MCP49x1(MCP49x1_Model model,
            int8_t        csPin,
            int8_t        sckPin,
            int8_t        mosiPin,
            int8_t        ldacPin = MCP49X1_LDAC_TIED_LOW,
            float         vref    = 3.3f);

    // ──────────────────────────────────────────────────────────────────────────
    // Initialisation / teardown
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * @brief Initialise GPIO pins and SPI peripheral.  Call once in setup().
     *
     * @param spiFreq  Hardware SPI clock in Hz. Max is 20 MHz per datasheet.
     *                 Default 8 MHz is a conservative safe choice.
     */
    void begin(uint32_t spiFreq = 8000000UL);

    /**
     * @brief Shutdown the device and release hardware SPI.
     */
    void end();

    // ──────────────────────────────────────────────────────────────────────────
    // Configuration (can be called any time after begin())
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * @brief Select output amplifier gain.
     * @param gain  GAIN_1X or GAIN_2X
     */
    void setGain(MCP49x1_Gain gain);
    MCP49x1_Gain getGain() const { return _gain; }

    /**
     * @brief Select VREF buffer mode.
     * @param mode  VREF_UNBUFFERED (default) or VREF_BUFFERED
     */
    void setVRefMode(MCP49x1_VRefMode mode);
    MCP49x1_VRefMode getVRefMode() const { return _vrefMode; }

    /**
     * @brief Update the VREF voltage used in voltage calculations.
     * @param vref  Voltage in volts
     */
    void setVRef(float vref);
    float getVRef() const { return _vref; }

    // ──────────────────────────────────────────────────────────────────────────
    // Output control
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * @brief Write a raw integer code to the DAC.
     *
     * The value is clamped to [0, maxCode].
     * For MCP4911 and MCP4901, the library automatically left-shifts the
     * data into the correct bit positions per the datasheet command register.
     *
     * @param code  12-bit: 0–4095 | 10-bit: 0–1023 | 8-bit: 0–255
     * @return MCP49x1_OK or MCP49x1_ERR_CLAMP
     */
    int setRaw(uint16_t code);

    /**
     * @brief Set output as a fraction of full scale.
     *
     * @param fraction  0.0 = 0 V output, 1.0 = VREF×Gain output (clamped)
     * @return MCP49x1_OK, MCP49x1_ERR_CLAMP, or MCP49x1_ERR_PARAM
     */
    int setPercent(float fraction);

    /**
     * @brief Set the output to a specific voltage.
     *
     * Voltage is clamped to [0 V, VREF × Gain].
     * Formula (from datasheet Eq. 4-1):
     *   VOUT = VREF × (D / 2^n) × Gain
     *   → D  = round(voltage / (VREF × Gain) × 2^n)
     *
     * @param voltage  Desired output in volts
     * @return MCP49x1_OK or MCP49x1_ERR_CLAMP
     */
    int setVoltage(float voltage);

    // ──────────────────────────────────────────────────────────────────────────
    // Shutdown control  (datasheet Section 4.2.4)
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * @brief Enter software shutdown mode.
     *
     * Most internal circuits are powered off.
     * VOUT is disconnected and pulled through ~500 kΩ.
     * The SPI interface stays active so wakeup() can restore operation.
     * Typical shutdown current: 3.3 µA @ 5V.
     */
    void shutdown();

    /**
     * @brief Wake the device from software shutdown.
     *
     * Sends a write command with SHDN=1 and restores the last code.
     * Settling time after wakeup is <10 µs per datasheet.
     */
    void wakeup();

    /** @brief Returns true if the device is currently in shutdown mode. */
    bool isShutdown() const { return _isShutdown; }

    // ──────────────────────────────────────────────────────────────────────────
    // LDAC control  (datasheet Section 3.5 and 5.2)
    // ──────────────────────────────────────────────────────────────────────────

    /**
     * @brief Pulse LDAC low to simultaneously latch all pending DAC values.
     *
     * Use this to synchronise multiple DACs:
     *   1. Disable auto-latch:  dac1.setAutoLatch(false); dac2.setAutoLatch(false);
     *   2. Load each DAC:       dac1.setVoltage(1.5f);    dac2.setVoltage(2.0f);
     *   3. Latch both at once:  dac1.latchOutput();
     *      (or drive LDAC pins together externally)
     *
     * Has no effect if ldacPin == MCP49X1_LDAC_TIED_LOW.
     */
    void latchOutput();

    /**
     * @brief Enable or disable automatic LDAC latching after every write.
     *
     * When true (default), latchOutput() is called at the end of every
     * setRaw / setVoltage / setPercent.
     * Set to false when loading multiple DACs for simultaneous output update.
     */
    void setAutoLatch(bool enable) { _autoLatch = enable; }
    bool getAutoLatch() const      { return _autoLatch; }

    // ──────────────────────────────────────────────────────────────────────────
    // Diagnostics / info
    // ──────────────────────────────────────────────────────────────────────────

    /** @brief DAC resolution in bits (8, 10, or 12). */
    uint8_t getResolutionBits() const { return _bits; }

    /** @brief Maximum DAC code (255, 1023, or 4095). */
    uint16_t getMaxCode() const { return _maxCode; }

    /** @brief Maximum output voltage = VREF × Gain. */
    float getMaxVoltage() const;

    /** @brief One LSB size in volts = VREF × Gain / 2^n. */
    float getLSB() const;

    /** @brief Last raw code sent to the DAC. */
    uint16_t getLastCode() const { return _lastCode; }

    /** @brief Calculated voltage for the last code written. */
    float getLastVoltage() const;

    /** @brief Model identifier (MCP4901, MCP4911, or MCP4921). */
    MCP49x1_Model getModel() const { return _model; }

    /**
     * @brief Print a summary of the current configuration to Serial.
     *        Requires that Serial has been initialised before calling.
     */
    void printConfig() const;

private:
    // ── Private helpers ───────────────────────────────────────────────────────
    void     _writeCommand(uint16_t command);
    void     _swSPITransfer16(uint16_t data);
    uint16_t _buildCommand(uint16_t code, bool active);
    void     _doLatch();

    // ── State ─────────────────────────────────────────────────────────────────
    MCP49x1_Model    _model;
    MCP49x1_Gain     _gain;
    MCP49x1_VRefMode _vrefMode;
    MCP49x1_SPIMode  _spiMode;

    int8_t   _csPin;
    int8_t   _ldacPin;
    int8_t   _sckPin;
    int8_t   _mosiPin;

    float    _vref;
    uint16_t _maxCode;
    uint8_t  _bits;
    uint32_t _spiFreq;

    uint16_t _lastCode;
    bool     _isShutdown;
    bool     _autoLatch;
    bool     _begun;
};

#endif // MCP49X1_H
