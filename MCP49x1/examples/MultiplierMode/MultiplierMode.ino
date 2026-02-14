/**
 * @file MultiplierMode.ino
 * @brief MCP49x1 – Multiplier / Programmable Attenuator mode
 *
 * In Multiplier Mode, an AC signal is fed into VREF.
 * The DAC code D controls the output attenuation / gain:
 *
 *   VOUT = VREF_signal × (D / 2^n) × Gain
 *
 * With Gain=1x on MCP4921 (12-bit):
 *   D = 0    → 0% of VREF signal  (fully attenuated)
 *   D = 2048 → 50% of VREF signal
 *   D = 4095 → ~100% of VREF signal
 *
 * Bandwidth in unbuffered mode:  ~450 kHz (gain=1x), ~400 kHz (gain=2x)
 * (datasheet Section 1.0 – Multiplier Mode parameters)
 *
 * In this demo we use a potentiometer (or analog input) to control
 * the DAC code, simulating a digitally-controlled volume/gain knob.
 *
 * Wiring:
 *   GPIO 18  → SCK
 *   GPIO 23  → SDI
 *   GPIO  5  → CS
 *   GND      → LDAC
 *   AC signal source → VREF  (e.g. function generator, audio line)
 *   GPIO 34  → Analog input (potentiometer wiper, 0–3.3 V)
 *   VDD / VSS / VOUT as normal
 */

#include <MCP49x1.h>

#define CS_PIN      5
#define POT_PIN     34    // ADC pin for potentiometer (12-bit ESP32 ADC)

MCP49x1 dac(MCP4921, CS_PIN, MCP49X1_LDAC_TIED_LOW, 3.3f);

// ESP32 ADC is 12-bit (0–4095), same range as MCP4921 codes — convenient!
const uint16_t ADC_MAX = 4095;

uint16_t lastCode    = 0xFFFF; // force first update
uint32_t printTimer  = 0;

void setup() {
    Serial.begin(115200);
    delay(500);

    dac.begin(8000000UL);

    // Use unbuffered VREF for widest frequency range in multiplier mode
    dac.setVRefMode(VREF_UNBUFFERED);
    dac.setGain(GAIN_1X);  // VOUT = VREF × D/4096

    analogReadResolution(12);  // match DAC resolution

    Serial.println("=== MCP49x1 Multiplier / Attenuator Demo ===");
    Serial.println("Connect AC signal to VREF pin.");
    Serial.println("Adjust potentiometer on GPIO34 to control attenuation.");
    Serial.println();
    dac.printConfig();
}

void loop() {
    // Read potentiometer → map directly to DAC code
    uint16_t adcVal = (uint16_t)analogRead(POT_PIN);

    // Only update DAC when value changes (reduces unnecessary SPI traffic)
    if (adcVal != lastCode) {
        lastCode = adcVal;
        dac.setRaw(adcVal);
    }

    // Print status every 500 ms
    uint32_t now = millis();
    if (now - printTimer >= 500) {
        printTimer = now;

        float attenPct = (float)dac.getLastCode() / 4095.0f * 100.0f;
        Serial.printf("Code: %4u  |  Attenuation: %6.2f%%  |  VOUT = VREF × %.4f\n",
                      dac.getLastCode(),
                      attenPct,
                      (float)dac.getLastCode() / 4095.0f);
    }

    // Small demo: auto-sweep if no serial commands and pot is at zero
    if (adcVal == 0 && Serial.available()) {
        char c = Serial.read();
        if (c == 'd') {
            Serial.println("Running attenuation sweep demo...");
            for (uint16_t code = 0; code <= 4095; code += 16) {
                dac.setRaw(code);
                delayMicroseconds(500);
            }
            for (uint16_t code = 4095; code > 0; code -= 16) {
                dac.setRaw(code);
                delayMicroseconds(500);
            }
            Serial.println("Sweep done.");
        }
    }
}
