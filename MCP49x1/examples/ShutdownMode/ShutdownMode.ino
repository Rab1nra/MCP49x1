/**
 * @file ShutdownMode.ino
 * @brief MCP49x1 – Software shutdown and wakeup demonstration
 *
 * When in shutdown:
 *   - Most internal circuits are off (~3.3 µA @ 5V typical)
 *   - VOUT is internally connected to ~500 kΩ to VSS
 *   - SPI interface stays active
 *
 * After wakeup:
 *   - Device restores the previously programmed code
 *   - Output settling time < 10 µs per datasheet
 *
 * Wiring: same as BasicDAC example.
 */

#include <MCP49x1.h>

#define CS_PIN    5
#define LDAC_PIN  15   // Optional: GPIO 15 for manual LDAC control

// Use LDAC pin for this demo to show manual latching
MCP49x1 dac(MCP4921, CS_PIN, LDAC_PIN, 3.3f);

void setup() {
    Serial.begin(115200);
    delay(500);

    dac.begin(8000000UL);
    dac.setGain(GAIN_1X);

    Serial.println("=== MCP49x1 Shutdown Mode Demo ===");
    dac.printConfig();
}

void loop() {
    // ── Step 1: Set a voltage and verify ────────────────────────────────
    Serial.println("\n[1] Setting output to 1.65 V ...");
    dac.setVoltage(1.65f);
    Serial.printf("    Output: %.4f V (code %u)\n",
                  dac.getLastVoltage(), dac.getLastCode());
    delay(2000);

    // ── Step 2: Enter shutdown ───────────────────────────────────────────
    Serial.println("[2] Entering SHUTDOWN mode ...");
    dac.shutdown();
    Serial.printf("    isShutdown = %s\n", dac.isShutdown() ? "true" : "false");
    Serial.println("    VOUT is now ~500 kΩ to GND — measure with DVM.");
    delay(3000);

    // ── Step 3: Wake up ──────────────────────────────────────────────────
    Serial.println("[3] Waking up from shutdown ...");
    dac.wakeup();
    Serial.printf("    Output restored: %.4f V (code %u)\n",
                  dac.getLastVoltage(), dac.getLastCode());
    delay(2000);

    // ── Step 4: Ramp while showing isShutdown state ──────────────────────
    Serial.println("[4] Voltage ramp 0 V → 3.3 V ...");
    for (float v = 0.0f; v <= 3.3f; v += 0.1f) {
        dac.setVoltage(v);
        Serial.printf("    %.2f V → code %u\n", dac.getLastVoltage(), dac.getLastCode());
        delay(100);
    }

    // ── Step 5: Multiple shutdown/wakeup cycles ───────────────────────────
    Serial.println("[5] Rapid shutdown / wakeup cycles (5x) ...");
    for (int i = 0; i < 5; i++) {
        dac.setVoltage(2.5f);
        delay(500);
        dac.shutdown();
        delay(500);
        dac.wakeup();
        Serial.printf("    Cycle %d: restored %.4f V\n", i + 1, dac.getLastVoltage());
    }

    Serial.println("\nLoop complete. Repeating in 3 s ...");
    delay(3000);
}
