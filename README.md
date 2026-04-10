# MCP49x1 — Arduino / ESP32 Library

Full-featured driver for the **Microchip MCP4901 / MCP4911 / MCP4921** family of single-channel SPI DACs.

| Device  | Resolution | Codes      | External VREF |
|---------|-----------|------------|---------------|
| MCP4901 |  8-bit    | 0 – 255    | ✅            |
| MCP4911 | 10-bit    | 0 – 1023   | ✅            |
| MCP4921 | 12-bit    | 0 – 4095   | ✅            |

---

## Features

- **Hardware SPI** (up to 20 MHz) and **Software (bit-bang) SPI** on any GPIO pins
- `setVoltage(float V)` — set output to an exact voltage
- `setPercent(float 0.0–1.0)` — fractional full-scale output
- `setRaw(uint16_t code)` — direct DAC code write
- **1x / 2x gain** selection (`setGain()`)
- **Buffered / Unbuffered VREF** mode (`setVRefMode()`)
- **Software Shutdown** / **Wakeup** (`shutdown()` / `wakeup()`)
- **LDAC pin support** for synchronous multi-DAC latching
- **Auto-latch control** for simultaneous update of multiple DACs
- `printConfig()` — dumps full configuration to Serial
- Status codes for out-of-range detection

---

## Installation

1. Download or clone this repository.
2. In the Arduino IDE: **Sketch → Include Library → Add .ZIP Library…** and select the folder.
3. Or copy the `MCP49x1` folder into your `Arduino/libraries/` directory.

---

## Wiring (ESP32 → MCP4921)

```
ESP32 GPIO 18  ──────  SCK   (pin 3)
ESP32 GPIO 23  ──────  SDI   (pin 4)
ESP32 GPIO  5  ──────  CS    (pin 2)
GND            ──────  LDAC  (pin 5)  ← tie to GND for immediate update
3.3V           ──────  VREF  (pin 6)
3.3V           ──────  VDD   (pin 1)
GND            ──────  VSS   (pin 7)
               ──────  VOUT  (pin 8)  ← your analog output
```

> ⚡ Add a **100 nF ceramic** + **10 µF tantalum** decoupling capacitor on VDD, as close to the chip as possible (datasheet Section 3.1).

---

## Quick Start

```cpp
#include <MCP49x1.h>

// MCP4921, CS=GPIO5, LDAC tied to GND, VREF=3.3V
MCP49x1 dac(MCP4921, 5, MCP49X1_LDAC_TIED_LOW, 3.3f);

void setup() {
    dac.begin();           // initialise SPI + GPIO
    dac.setGain(GAIN_1X);  // VOUT max = VREF = 3.3V
}

void loop() {
    dac.setVoltage(1.65f); // set output to 1.65 V
    delay(1000);
    dac.setPercent(0.75f); // 75% of full scale → ~2.475 V
    delay(1000);
    dac.setRaw(2048);      // raw code → ~1.649 V
    delay(1000);
}
```

---

## API Reference

### Constructors

```cpp
// Hardware SPI
MCP49x1(MCP49x1_Model model, int8_t csPin,
        int8_t ldacPin = MCP49X1_LDAC_TIED_LOW,
        float vref = 3.3f);

// Software SPI (bit-bang)
MCP49x1(MCP49x1_Model model, int8_t csPin,
        int8_t sckPin, int8_t mosiPin,
        int8_t ldacPin = MCP49X1_LDAC_TIED_LOW,
        float vref = 3.3f);
```

### Initialisation

| Method | Description |
|--------|-------------|
| `begin(uint32_t spiFreq = 8000000)` | Initialise GPIO + SPI. Max 20 MHz. |
| `end()` | Shutdown device and release SPI. |

### Configuration

| Method | Description |
|--------|-------------|
| `setGain(GAIN_1X / GAIN_2X)` | Output amplifier gain (1x or 2x). |
| `getGain()` | Returns current gain. |
| `setVRefMode(VREF_UNBUFFERED / VREF_BUFFERED)` | VREF input buffer mode. |
| `setVRef(float volts)` | Update VREF used in voltage math. |
| `getVRef()` | Returns current VREF. |

### Output Control

| Method | Returns | Description |
|--------|---------|-------------|
| `setRaw(uint16_t code)` | `MCP49x1_Status` | Write raw DAC code (clamped). |
| `setPercent(float 0.0–1.0)` | `MCP49x1_Status` | Fractional full-scale output. |
| `setVoltage(float volts)` | `MCP49x1_Status` | Set exact output voltage. |

Return values: `MCP49x1_OK (0)`, `MCP49x1_ERR_CLAMP (-1)`, `MCP49x1_ERR_PARAM (-2)`.

### Shutdown

| Method | Description |
|--------|-------------|
| `shutdown()` | Enter low-power shutdown (~3.3 µA). VOUT → 500 kΩ to GND. |
| `wakeup()` | Exit shutdown, restore last code. Settles in < 10 µs. |
| `isShutdown()` | Returns `true` if currently shut down. |

### LDAC Control

| Method | Description |
|--------|-------------|
| `latchOutput()` | Pulse LDAC low to latch pending DAC value. |
| `setAutoLatch(bool)` | `true` (default) = latch on every write. `false` = manual latch. |
| `getAutoLatch()` | Returns current auto-latch setting. |

### Diagnostics

| Method | Description |
|--------|-------------|
| `getResolutionBits()` | 8, 10, or 12. |
| `getMaxCode()` | 255, 1023, or 4095. |
| `getMaxVoltage()` | VREF × Gain. |
| `getLSB()` | One LSB in volts. |
| `getLastCode()` | Last code sent to DAC. |
| `getLastVoltage()` | Calculated voltage for last code. |
| `getModel()` | Returns MCP4901 / MCP4911 / MCP4921. |
| `printConfig()` | Print full config table to Serial. |

---

## Output Voltage Formula

From datasheet Equation 4-1:

```
VOUT = VREF × (D / 2^n) × Gain

Where:
  n    = resolution (8, 10, or 12)
  D    = DAC code
  Gain = 1 (GA bit = 1) or 2 (GA bit = 0)
```

**LSB sizes with VREF = 3.3 V, Gain = 1x:**

| Model   | Resolution | LSB Size  |
|---------|-----------|-----------|
| MCP4901 |  8-bit    | 12.89 mV  |
| MCP4911 | 10-bit    |  3.22 mV  |
| MCP4921 | 12-bit    |  0.806 mV |

---

## Synchronous Multi-DAC Update

To update two (or more) DAC outputs simultaneously, use the LDAC pin:

```cpp
// Wire both DAC LDAC pins to the same ESP32 GPIO (e.g. GPIO 15)
MCP49x1 dac1(MCP4921, 5,  15, 3.3f);  // CS=5,  LDAC=15
MCP49x1 dac2(MCP4921, 17, 15, 3.3f);  // CS=17, LDAC=15

void setup() {
    dac1.begin(); dac2.begin();
    dac1.setAutoLatch(false);   // prevent auto-latch
    dac2.setAutoLatch(false);
}

void loop() {
    dac1.setVoltage(1.0f);  // load into input register only
    dac2.setVoltage(2.0f);  // load into input register only
    dac1.latchOutput();     // pulse LDAC → both outputs update at the same time
}
```

---

## Examples

| Example | Description |
|---------|-------------|
| `BasicDAC` | Minimal raw / percent / voltage output demo |
| `VoltageOutput` | All three models + software SPI + 2x gain |
| `WaveformGenerator` | Sine, triangle, sawtooth, square wave output |
| `ShutdownMode` | Shutdown / wakeup power management demo |
| `MultiplierMode` | Programmable attenuator with AC VREF input |

---

## Datasheet

Microchip DS22248A — MCP4901/4911/4921 8/10/12-Bit Voltage Output DAC with SPI Interface.

Key specifications:
- SPI clock: up to **20 MHz**
- Settling time: **4.5 µs** typical
- Shutdown current: **3.3 µA** typical @ 5V
- Output swing: **10 mV** to **VDD − 40 mV** (rail-to-rail)
- Supply voltage: **2.7 V to 5.5 V**
- Temperature range: **−40°C to +125°C**

---

## License

 free to use, modify, and distribute.
