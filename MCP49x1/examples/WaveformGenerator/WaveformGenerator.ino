/**
 * @file WaveformGenerator.ino
 * @brief MCP49x1 – Waveform generator: Sine, Triangle, Sawtooth, Square
 *
 * Uses the ESP32 hardware SPI for maximum speed.
 * Demonstrates high-speed raw-code output for waveform synthesis.
 *
 * Wiring (same as BasicDAC example):
 *   GPIO 18  → SCK
 *   GPIO 23  → SDI (MOSI)
 *   GPIO  5  → CS
 *   GND      → LDAC   (tied low for immediate update)
 *   3.3V     → VREF
 *   3.3V     → VDD
 *   GND      → VSS
 *
 * On Serial Monitor: type a character to change waveform:
 *   's' → Sine
 *   't' → Triangle
 *   'w' → Sawtooth
 *   'q' → Square
 *   '+' → Increase frequency
 *   '-' → Decrease frequency
 */

#include <MCP49x1.h>
#include <math.h>

#define CS_PIN   5

MCP49x1 dac(MCP4921, CS_PIN, MCP49X1_LDAC_TIED_LOW, 3.3f);

// ── Waveform parameters ───────────────────────────────────────────────────
#define SAMPLES      64        // Points per cycle
#define MAX_CODE     4095      // MCP4921 12-bit full scale
#define HALF_CODE    2048      // Midpoint

enum WaveShape { SINE, TRIANGLE, SAWTOOTH, SQUARE };
WaveShape currentWave = SINE;

uint32_t stepDelayUs = 100;    // microseconds between samples → ~156 Hz @ 64 pts

// Pre-computed sine table (0 to 4095)
uint16_t sineTable[SAMPLES];

void buildSineTable() {
    for (int i = 0; i < SAMPLES; i++) {
        float rad     = 2.0f * M_PI * i / SAMPLES;
        sineTable[i]  = (uint16_t)roundf((sinf(rad) + 1.0f) * 0.5f * MAX_CODE);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    dac.begin(20000000UL);  // 20 MHz – max allowed by datasheet
    dac.setGain(GAIN_1X);

    buildSineTable();

    Serial.println("=== MCP49x1 Waveform Generator ===");
    Serial.println("Commands: s=Sine  t=Triangle  w=Sawtooth  q=Square  +=faster  -=slower");
}

void outputSine(uint32_t delayUs) {
    for (int i = 0; i < SAMPLES; i++) {
        dac.setRaw(sineTable[i]);
        delayMicroseconds(delayUs);
    }
}

void outputTriangle(uint32_t delayUs) {
    // Rise: 0 → MAX
    for (int i = 0; i <= MAX_CODE; i += MAX_CODE / SAMPLES) {
        dac.setRaw((uint16_t)i);
        delayMicroseconds(delayUs);
    }
    // Fall: MAX → 0
    for (int i = MAX_CODE; i >= 0; i -= MAX_CODE / SAMPLES) {
        dac.setRaw((uint16_t)i);
        delayMicroseconds(delayUs);
    }
}

void outputSawtooth(uint32_t delayUs) {
    uint16_t step = MAX_CODE / SAMPLES;
    for (uint16_t i = 0; i <= MAX_CODE; i += step) {
        dac.setRaw(i);
        delayMicroseconds(delayUs);
    }
}

void outputSquare(uint32_t delayUs) {
    uint32_t halfPeriod = delayUs * SAMPLES / 2;
    dac.setRaw(MAX_CODE);
    delayMicroseconds(halfPeriod);
    dac.setRaw(0);
    delayMicroseconds(halfPeriod);
}

void handleSerial() {
    if (!Serial.available()) return;
    char c = Serial.read();
    switch (c) {
        case 's':
            currentWave = SINE;
            Serial.println("Waveform → Sine");
            break;
        case 't':
            currentWave = TRIANGLE;
            Serial.println("Waveform → Triangle");
            break;
        case 'w':
            currentWave = SAWTOOTH;
            Serial.println("Waveform → Sawtooth");
            break;
        case 'q':
            currentWave = SQUARE;
            Serial.println("Waveform → Square");
            break;
        case '+':
            if (stepDelayUs > 10) stepDelayUs -= 10;
            Serial.printf("Step delay → %u µs\n", stepDelayUs);
            break;
        case '-':
            stepDelayUs += 10;
            Serial.printf("Step delay → %u µs\n", stepDelayUs);
            break;
    }
}

void loop() {
    handleSerial();

    switch (currentWave) {
        case SINE:      outputSine(stepDelayUs);      break;
        case TRIANGLE:  outputTriangle(stepDelayUs);  break;
        case SAWTOOTH:  outputSawtooth(stepDelayUs);  break;
        case SQUARE:    outputSquare(stepDelayUs);     break;
    }
}
