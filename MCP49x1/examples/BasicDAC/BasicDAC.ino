/**
 * @file BasicDAC.ino
 * @brief MCP49x1 – Minimal "hello world" DAC example for ESP32
 *
 * Wiring (ESP32 → MCP4921):
 *   GPIO 18  → SCK
 *   GPIO 23  → SDI (MOSI)
 *   GPIO  5  → CS
 *   GND      → LDAC  (tie to GND for immediate output update)
 *   3.3V     → VREF
 *   3.3V     → VDD
 *   GND      → VSS
 *
 * Expected output on VOUT:
 *   ~0.825 V (25 % of 3.3 V)
 *   ~1.650 V (50 % of 3.3 V)
 *   ~2.475 V (75 % of 3.3 V)
 *   ~3.300 V (100% of 3.3 V)
 */

#include <MCP49x1.h>

// ── Pin definitions ────────────────────────────────────────────────────────
#define CS_PIN   5     // GPIO 5  → CS
// SCK and MOSI are handled automatically by the ESP32 hardware SPI (pins 18, 23)

// ── Create DAC instance ────────────────────────────────────────────────────
//   MCP4921 = 12-bit,  CS pin = 5,  LDAC tied to GND,  VREF = 3.3 V
MCP49x1 dac(MCP4921, CS_PIN, MCP49X1_LDAC_TIED_LOW, 3.3f);

void setup() {
    Serial.begin(115200);
    delay(500);

    // Initialise the DAC (8 MHz SPI clock)
    dac.begin(8000000UL);

    // Optional: use 1x gain so VOUT max = VREF = 3.3 V
    dac.setGain(GAIN_1X);

    // Print configuration to Serial Monitor
    dac.printConfig();

    Serial.println("MCP49x1 BasicDAC example started.");
}

void loop() {
    // ── Set output by raw code ───────────────────────────────────────────
    Serial.println("\n--- setRaw ---");

    dac.setRaw(0);
    Serial.printf("  Code 0      → VOUT ≈ %.4f V\n", dac.getLastVoltage());
    delay(1000);

    dac.setRaw(1023);
    Serial.printf("  Code 1023   → VOUT ≈ %.4f V\n", dac.getLastVoltage());
    delay(1000);

    dac.setRaw(2048);
    Serial.printf("  Code 2048   → VOUT ≈ %.4f V\n", dac.getLastVoltage());
    delay(1000);

    dac.setRaw(4095);
    Serial.printf("  Code 4095   → VOUT ≈ %.4f V\n", dac.getLastVoltage());
    delay(1000);

    // ── Set output by percentage ─────────────────────────────────────────
    Serial.println("\n--- setPercent ---");

    dac.setPercent(0.25f);
    Serial.printf("  25%%  → VOUT ≈ %.4f V\n", dac.getLastVoltage());
    delay(1000);

    dac.setPercent(0.50f);
    Serial.printf("  50%%  → VOUT ≈ %.4f V\n", dac.getLastVoltage());
    delay(1000);

    dac.setPercent(0.75f);
    Serial.printf("  75%%  → VOUT ≈ %.4f V\n", dac.getLastVoltage());
    delay(1000);

    dac.setPercent(1.00f);
    Serial.printf("  100%% → VOUT ≈ %.4f V\n", dac.getLastVoltage());
    delay(1000);

    // ── Set output by voltage ────────────────────────────────────────────
    Serial.println("\n--- setVoltage ---");

    float targets[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.3f};
    for (float v : targets) {
        int status = dac.setVoltage(v);
        Serial.printf("  Target %.2f V → actual %.4f V  (status %d)\n",
                      v, dac.getLastVoltage(), status);
        delay(800);
    }
}
