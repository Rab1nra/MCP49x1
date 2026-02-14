/**
 * @file VoltageOutput.ino
 * @brief MCP49x1 – Voltage output demo covering all three models
 *        (MCP4901 / MCP4911 / MCP4921) and software SPI mode.
 *
 * Demonstrates:
 *   - How to use all three device variants
 *   - Software (bit-bang) SPI on any GPIO pins
 *   - Gain 2x mode (output range 0 – 2×VREF)
 *   - Multiple DACs on the same SPI bus using separate CS pins
 *   - Synchronous multi-DAC update using LDAC
 *
 * Wiring — two DACs sharing HW SPI bus:
 *   GPIO 18  → SCK      (both chips)
 *   GPIO 23  → SDI      (both chips)
 *   GPIO  5  → CS  (DAC 1 – MCP4921)
 *   GPIO 17  → CS  (DAC 2 – MCP4921)
 *   GPIO 15  → LDAC     (both chips tied together)
 *   3.3V     → VREF, VDD  (both chips)
 *   GND      → VSS        (both chips)
 */

#include <MCP49x1.h>

// ── DAC 1 — MCP4921, hardware SPI ──────────────────────────────────────────
#define CS1_PIN    5
#define CS2_PIN   17
#define LDAC_PIN  15

// Both DACs share the same LDAC pin for synchronous update
MCP49x1 dac1(MCP4921, CS1_PIN, LDAC_PIN, 3.3f);
MCP49x1 dac2(MCP4921, CS2_PIN, LDAC_PIN, 3.3f);

// ── DAC 3 — MCP4901 (8-bit), software SPI on arbitrary pins ───────────────
#define SW_CS_PIN   16
#define SW_SCK_PIN  25
#define SW_MOSI_PIN 26

MCP49x1 dac3(MCP4901, SW_CS_PIN, SW_SCK_PIN, SW_MOSI_PIN,
             MCP49X1_LDAC_TIED_LOW, 3.3f);

// ── Helper: print a comparison table ──────────────────────────────────────
void printResolutionComparison(float vref) {
    Serial.println("\n╔══════════╦════════╦══════════╦═══════════╦══════════════╗");
    Serial.println("║ Model    ║ Bits   ║ Max Code ║ Max VOUT  ║ LSB Size     ║");
    Serial.println("╠══════════╬════════╬══════════╬═══════════╬══════════════╣");

    MCP49x1_Model models[] = {MCP4901, MCP4911, MCP4921};
    const char*   names[]  = {"MCP4901", "MCP4911", "MCP4921"};

    for (int i = 0; i < 3; i++) {
        uint8_t  bits    = (uint8_t)models[i];
        uint16_t maxCode = (1u << bits) - 1u;
        float    lsb     = vref / (float)(1u << bits); // gain=1x
        Serial.printf("║ %-8s ║ %-6u ║ %-8u ║ %-9.4f ║ %-12.4f ║\n",
                      names[i], bits, maxCode, vref, lsb * 1000.0f);
    }
    Serial.println("╚══════════╩════════╩══════════╩═══════════╩══════════════╝");
    Serial.println("  (VOUT max in V, LSB in mV, gain=1x, VREF=" +
                   String(vref, 2) + "V)");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // Initialise all three DACs
    dac1.begin(8000000UL);
    dac2.begin(8000000UL);
    dac3.begin();                   // SW SPI — no frequency parameter

    dac1.setGain(GAIN_1X);
    dac2.setGain(GAIN_1X);
    dac3.setGain(GAIN_1X);

    // Disable auto-latch on both shared-bus DACs for synchronous update demo
    dac1.setAutoLatch(false);
    dac2.setAutoLatch(false);

    Serial.println("=== MCP49x1 Multi-Model Voltage Output Demo ===");
    printResolutionComparison(3.3f);
}

void loop() {
    // ── Demo 1: Set specific voltages on DAC1 and DAC2 ─────────────────
    Serial.println("\n[1] Setting individual voltages...");

    float v1 = 1.65f, v2 = 2.5f;
    dac1.setVoltage(v1);   // Loaded into input register (no latch yet)
    dac2.setVoltage(v2);   // Loaded into input register (no latch yet)

    // Now pulse LDAC to update BOTH outputs simultaneously
    dac1.latchOutput();    // This pulses LDAC pin (shared by both DACs)

    Serial.printf("  DAC1: target=%.2f  actual=%.4f V\n", v1, dac1.getLastVoltage());
    Serial.printf("  DAC2: target=%.2f  actual=%.4f V\n", v2, dac2.getLastVoltage());
    delay(2000);

    // ── Demo 2: Voltage sweep on DAC1 ──────────────────────────────────
    Serial.println("[2] DAC1 voltage sweep 0 → 3.3 V...");
    dac1.setAutoLatch(true);   // re-enable for simple use
    for (float v = 0.0f; v <= 3.3f; v += 0.05f) {
        dac1.setVoltage(v);
        delay(20);
    }
    dac1.setAutoLatch(false);  // back off for sync demos
    Serial.println("  Sweep complete.");

    // ── Demo 3: DAC3 (8-bit, SW SPI) ────────────────────────────────────
    Serial.println("[3] MCP4901 (8-bit, SW SPI) output steps...");
    float steps[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.3f};
    for (float v : steps) {
        dac3.setVoltage(v);
        Serial.printf("  8-bit DAC: target=%.1f → code=%u → actual=%.4f V  (LSB=%.1f mV)\n",
                      v, dac3.getLastCode(), dac3.getLastVoltage(),
                      dac3.getLSB() * 1000.0f);
        delay(500);
    }

    // ── Demo 4: 2x Gain mode ─────────────────────────────────────────────
    Serial.println("[4] Gain 2x: VOUT = 2×VREF×D/4096 (max = 6.6 V, capped at VDD)");
    dac1.setAutoLatch(true);
    dac1.setGain(GAIN_2X);
    Serial.printf("  Max VOUT with Gain=2x: %.4f V (but VDD limits to 3.3V!)\n",
                  dac1.getMaxVoltage());
    dac1.setVoltage(1.0f);
    Serial.printf("  1.00 V → code=%u → actual=%.4f V\n",
                  dac1.getLastCode(), dac1.getLastVoltage());
    dac1.setVoltage(1.5f);
    Serial.printf("  1.50 V → code=%u → actual=%.4f V\n",
                  dac1.getLastCode(), dac1.getLastVoltage());

    // Restore gain 1x
    dac1.setGain(GAIN_1X);
    dac1.setAutoLatch(false);

    Serial.println("\nLoop done. Restarting in 3 s...");
    delay(3000);
}
