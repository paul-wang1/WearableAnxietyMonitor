/*
 * ESP32-C3 PPG Stress Monitor
 * 
 * MAX30102 heart rate + HRV stress classifier with BLE output.
 * Only sends BLE notification when BOTH thresholds are exceeded.
 *
 * Thresholds (from papers):
 *   - HR:    +10 BPM above baseline (Juliane Hellhammer)
 *   - RMSSD: -7.9ms AND -17% from baseline (Tarbell et al. 2017)
 * 
 * HRV method adapted from Stanford HeartRateSensor implementation.
 */
#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Config

// UUIDs from Paul have to match the app
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Sampling
#define SR 50
#define CALIB_SEC 60
#define SEND_INTERVAL_MS 300000UL

// Stress thresholds
#define HR_DELTA_BPM  10.0f
#define HRV_DELTA_MS   7.9f
#define HRV_DELTA_PCT  0.17f

// HR averaging (from Stanford)
#define RATE_SIZE 4
byte rates[RATE_SIZE];
byte rateSpot = 0;
int beatAvg = 0;
float beatsPerMinute = 0;
long lastBeat = 0;

// HRV tracking (from Stanford) - stores successive differences, not RR intervals
#define NUM_HRV_SAMPLES 30
#define MAX_HRV_INTERVAL 200  // max reasonable successive difference in ms
int16_t HRintervals[NUM_HRV_SAMPLES];
uint8_t hrvIndex = 0;
int16_t lastDelta = 0;
float RMSSD = 0.0f;

// Max sensor
MAX30105 sensor;

// BLE
BLEServer         *pServer         = nullptr;
BLECharacteristic *pCharacteristic = nullptr;
bool deviceConnected    = false;
bool oldDeviceConnected = false;

// Baseline
float baselineHR    = 0.0f;
float baselineRMSSD = 0.0f;
bool  calibrated    = false;

unsigned long lastSendMs = 0;

// GSR TODO
float gsr = 0.0f;

// BLE callbacks
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) override {
        deviceConnected = true;
        Serial.println("[BLE] Client connected");
    }
    void onDisconnect(BLEServer *pServer) override {
        deviceConnected = false;
        Serial.println("[BLE] Client disconnected");
    }
};

void resetBeatValues() {
    beatsPerMinute = 0;
    beatAvg = 0;
    lastBeat = 0;
    lastDelta = 0;
    hrvIndex = 0;
    RMSSD = 0;
    rateSpot = 0;
    for (int i = 0; i < RATE_SIZE; i++) rates[i] = 0;
    for (int i = 0; i < NUM_HRV_SAMPLES; i++) HRintervals[i] = 0;
}

// Process beat detection and HRV — adapted from Stanford
void processBeat(long irValue) {
    if (irValue < 50000) return;  // No finger

    if (checkForBeat(irValue)) {
        long delta = millis() - lastBeat;
        lastBeat = millis();

        beatsPerMinute = 60.0f / (delta / 1000.0f);

        if (beatsPerMinute > 20 && beatsPerMinute < 255) {
            // HR averaging
            rates[rateSpot++] = (byte)beatsPerMinute;
            rateSpot %= RATE_SIZE;

            beatAvg = 0;
            for (byte x = 0; x < RATE_SIZE; x++)
                beatAvg += rates[x];
            beatAvg /= RATE_SIZE;

            // HRV: store successive DIFFERENCE (Stanford method)
            int16_t interval = delta - lastDelta;
            lastDelta = delta;

            if (abs(interval) < MAX_HRV_INTERVAL) {
                HRintervals[hrvIndex++] = interval;
                hrvIndex %= NUM_HRV_SAMPLES;

                // RMSSD calculation
                float sum = 0.0f;
                for (int i = 0; i < NUM_HRV_SAMPLES; i++) {
                    sum += (float)HRintervals[i] * HRintervals[i];
                }
                RMSSD = sqrtf(sum / (NUM_HRV_SAMPLES - 1));
            }
        }
    }
}

// Calibration
void runCalibration(int retryCount = 0) {
    Serial.println("\n CALIBRATION");
    Serial.println("Keep finger on sensor. Collecting baseline for 60 seconds...");

    resetBeatValues();
    unsigned long start = millis();
    unsigned long lastPrint = 0;

    while (millis() - start < (unsigned long)(CALIB_SEC * 1000UL)) {
        long irValue = sensor.getIR();
        processBeat(irValue);

        unsigned long elapsed = millis() - start;
        if (elapsed - lastPrint >= 10000) {
            Serial.printf("  %lu / %d sec — HR: %d BPM, HRV: %.1f ms\n",
                          elapsed / 1000, CALIB_SEC, beatAvg, RMSSD);
            lastPrint = elapsed;
        }

        delay(20);  // 50 Hz
    }

    if (beatAvg < 40 || beatAvg > 180 || RMSSD < 1.0f) {
        if (retryCount < 1) {
            Serial.println("[CALIB] Failed — retrying in 5s");
            delay(5000);
            runCalibration(retryCount + 1);
        } else {
            Serial.println("[CALIB] Failed twice — halting");
            while (1) delay(1000);
        }
        return;
    }

    baselineHR    = beatAvg;
    baselineRMSSD = RMSSD;
    calibrated    = true;

    Serial.println("CALIBRATION COMPLETE");
    Serial.printf("  Baseline HR:    %.1f BPM\n", baselineHR);
    Serial.printf("  Baseline RMSSD: %.1f ms\n", baselineRMSSD);
    Serial.printf("  Stress triggers when: HR > %.1f BPM AND RMSSD < %.1f ms\n",
                  baselineHR + HR_DELTA_BPM,
                  baselineRMSSD - HRV_DELTA_MS);

    resetBeatValues();
}

// BLE setup
void setupBLE() {
    BLEDevice::init("ESP32C3-Paul");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Advertising as 'ESP32C3-Paul'");
}

// SETUP
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32-C3 Stress Monitor ===");

    // Sensor init — STANDARD speed + defaults (from Stanford)
    Wire.begin(21, 22);
    if (!sensor.begin(Wire, I2C_SPEED_STANDARD)) {
        Serial.println("[ERROR] MAX30102 not found");
        while (1);
    }
    sensor.setup();  // Use defaults — they're well tuned
    Serial.println("[SENSOR] MAX30102 ready");

    setupBLE();

    delay(2000);
    runCalibration();

    lastSendMs = millis();
}

// MAIN LOOP
void loop() {
    long irValue = sensor.getIR();
    processBeat(irValue);

    // Every 5 minutes: check for stress
    unsigned long now = millis();
    if (calibrated && (now - lastSendMs) >= SEND_INTERVAL_MS) {
        lastSendMs = now;

        float hr = beatAvg;
        float hrv = RMSSD;

        if (hr < 40 || hr > 180) {
            Serial.println("[SKIP] Invalid HR");
            delay(20);
            return;
        }

        bool hrElevated = (hr - baselineHR) > HR_DELTA_BPM;

        float rmssdDrop    = baselineRMSSD - hrv;
        float rmssdDropPct = rmssdDrop / baselineRMSSD;
        bool hrvLow = (rmssdDrop > HRV_DELTA_MS) && (rmssdDropPct > HRV_DELTA_PCT);

        Serial.println("\n VITALS ");
        Serial.printf("  HR:    %.1f BPM  (baseline: %.1f)\n", hr, baselineHR);
        Serial.printf("  RMSSD: %.1f ms   (baseline: %.1f)\n", hrv, baselineRMSSD);
        Serial.printf("  HR elevated: %s | HRV low: %s\n",
                      hrElevated ? "YES" : "no", hrvLow ? "YES" : "no");

        if (hrElevated && hrvLow) {
            Serial.println("  >> STRESS DETECTED");

            if (deviceConnected) {
                char payload[128];
                snprintf(payload, sizeof(payload),
                    "{\"gsr\": %.2f, \"hr\": %.1f, \"hrv\": %.1f}",
                    gsr, hr, hrv);

                pCharacteristic->setValue(payload);
                pCharacteristic->notify();
                Serial.printf("[TX] %s\n", payload);
            } else {
                Serial.println("[BLE] No client — skipped");
            }
        } else {
            Serial.println("  >> Normal state — no notification");
        }
    }

    // BLE reconnect
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("[BLE] Restarted advertising");
        oldDeviceConnected = false;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = true;
    }

    // Test: press 's' to send fake stress
    if (Serial.available()) {
        char c = Serial.read();
        if (c == 's' || c == 'S') {
            Serial.println("\n[TEST] Sending fake stress event");
            if (deviceConnected) {
                char payload[128];
                snprintf(payload, sizeof(payload),
                    "{\"gsr\": 0.65, \"hr\": 95.0, \"hrv\": 25.0}");
                pCharacteristic->setValue(payload);
                pCharacteristic->notify();
                Serial.printf("[TX] %s\n", payload);
            } else {
                Serial.println("[BLE] No client connected");
            }
        }
    }

    delay(20);  // 50 Hz
}
