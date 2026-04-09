/*
 * ESP32-C3 PPG Stress Monitor
 * 
 * MAX30102 heart rate + HRV stress classifier with BLE output.
 * Only sends BLE notification when BOTH thresholds are exceeded.
 *
 * Thresholds (from papers):
 *   - HR:    +10 BPM above baseline (Juliane Hellhammer conservative vs 19.98 mean)
 *   - RMSSD: -7.9ms AND -17% from baseline (Tarbell et al. 2017)
 * 
 * HRV method switched from manual peak detection on raw buffer to
 * checkForBeat() from heartRate.h (SparkFun MAX3010x library).
 * This gives cleaner RR intervals directly from the library's
 * zero-crossing detector, which is tuned for MAX30102 PPG signals.
 * RR intervals are accumulated in a 50-slot rolling buffer across
 * the full 5-minute window for more reliable RMSSD.
 */
#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"   // checkForBeat() — bundled with SparkFun MAX3010x library
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Config

// UUIDs from Paul have to match the app which it does
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Sampling param we can always change 
#define SR 50 // samples/sec (delay(20) = 50 Hz)
#define CALIB_SEC 60 // calibration window (seconds)
#define SEND_INTERVAL_MS 300000UL  // check every 5 minutes

// Rolling RR interval buffer — 50 slots covers ~60 beats at 72 BPM
// giving plenty of successive differences for reliable RMSSD
#define RR_BUF_SIZE 50

// Stress thresholds CHANGE BASED OFF OF PAPERS BUT SEEM GOOD
#define HR_DELTA_BPM  10.0f    // HR must rise by this much
#define HRV_DELTA_MS   7.9f   // RMSSD must drop by this much (absolute)
#define HRV_DELTA_PCT  0.17f  // RMSSD must drop by this much (relative)

// Max sensor init
MAX30105 sensor;

// BLE stuff
BLEServer         *pServer         = nullptr;
BLECharacteristic *pCharacteristic = nullptr;
bool deviceConnected    = false;
bool oldDeviceConnected = false;

// Baseline set during calibration
float baselineHR    = 0.0f;
float baselineRMSSD = 0.0f;
bool  calibrated    = false;

// Rolling RR interval buffer (seconds between beats)
float rrBuf[RR_BUF_SIZE];
int   rrHead  = 0;
int   rrCount = 0;

// Beat timing for RR calculation
long lastBeatMs = 0;

unsigned long lastSendMs = 0;

// GSR TODO grove sensor stuff
float gsr = 0.0f;

// BLE stuff
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

// DSP on chip funcs

// Push one validated RR interval into the rolling buffer.
// Outlier rejection: must be within 20% of previous interval
// to discard missed/double beats.
void pushRR(float rr) {
    if (rrCount > 0) {
        float prev = rrBuf[(rrHead - 1 + RR_BUF_SIZE) % RR_BUF_SIZE];
        if (rr < prev * 0.80f || rr > prev * 1.20f) {
            Serial.printf("[RR] Rejected outlier: %.0f ms\n", rr * 1000);
            return;
        }
    }
    rrBuf[rrHead] = rr;
    rrHead = (rrHead + 1) % RR_BUF_SIZE;
    if (rrCount < RR_BUF_SIZE) rrCount++;
}

// Compute HR and RMSSD from the current rolling RR buffer.
// Returns false if not enough intervals.
bool computeVitalsFromRR(float &hr, float &rmssd) {
    if (rrCount < 4) return false;

    int n = rrCount;

    // Pull intervals in chronological order from circular buffer
    float rr[RR_BUF_SIZE];
    for (int i = 0; i < n; i++)
        rr[i] = rrBuf[(rrHead - n + i + RR_BUF_SIZE) % RR_BUF_SIZE];

    // HR from mean RR
    float rrSum = 0;
    for (int i = 0; i < n; i++) rrSum += rr[i];
    float meanRR = rrSum / n;
    hr = 60.0f / meanRR;
    if (hr < 40.0f || hr > 180.0f) return false;

    // THIS IS AN ISSUE
    // RMSSD from successive cleaned RR differences
    // Using library beat detection gives cleaner intervals than
    // manual peak finding, so RMSSD is more stable here
    float sq = 0.0f;
    int pairs = 0;
    for (int i = 1; i < n; i++) {
        float diff = rr[i] - rr[i-1];
        sq += diff * diff;
        pairs++;
    }
    rmssd = (pairs > 0) ? sqrtf(sq / pairs) * 1000.0f : 0.0f;

    Serial.printf("[DEBUG] RR: %d intervals, mean: %.0f ms\n", n, meanRR * 1000);

    return true;
}

// Calibration 1 time at start
// Uses checkForBeat() to collect RR intervals over 60 seconds
// instead of raw sample buffer — more reliable baseline
void runCalibration(int retryCount = 0) {
    Serial.println("\n CALIBRATION");
    Serial.println("Keep finger on sensor. Collecting baseline for 60 seconds...");

    // Reset RR buffer for clean calibration
    rrHead  = 0;
    rrCount = 0;
    lastBeatMs = 0;

    unsigned long start = millis();

    while (millis() - start < (unsigned long)(CALIB_SEC * 1000UL)) {
        long irValue = sensor.getIR();

        // Finger check
        if (irValue < 50000) {
            Serial.println("[WARN] No finger — keep finger on sensor");
            delay(20);
            continue;
        }

        if (checkForBeat(irValue)) {
            long now = millis();
            if (lastBeatMs > 0) {
                float rr  = (now - lastBeatMs) / 1000.0f;
                float bpm = 60.0f / rr;
                if (bpm > 40.0f && bpm < 180.0f)
                    pushRR(rr);
            }
            lastBeatMs = now;
        }

        // Progress every 10 seconds
        unsigned long elapsed = millis() - start;
        if (elapsed % 10000 < 20)
            Serial.printf("  %lu / %d seconds (%d beats)\n",
                          elapsed / 1000, CALIB_SEC, rrCount);

        delay(20);  // 50 Hz
    }

    float hr, rmssd;
    if (!computeVitalsFromRR(hr, rmssd)) {
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

    baselineHR    = hr;
    baselineRMSSD = rmssd;
    calibrated    = true;

    Serial.println("CALIBRATION COMPLETE");
    Serial.printf("  Baseline HR:    %.1f BPM\n", baselineHR);
    Serial.printf("  Baseline RMSSD: %.1f ms\n", baselineRMSSD); // this one always tweaking
    Serial.printf("  Stress triggers when: HR > %.1f BPM AND RMSSD < %.1f ms\n",
                  baselineHR + HR_DELTA_BPM,
                  baselineRMSSD - HRV_DELTA_MS);

    // Reset buffer so loop() starts fresh after calibration
    rrHead  = 0;
    rrCount = 0;
    lastBeatMs = 0;
}

// BLE setup stuff
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
    // Helps with iPhone connection issues:
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

    // Sensor init
    Wire.begin(21, 22);
    if (!sensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("[ERROR] MAX30102 not found");
        while (1);
    }
    sensor.setup(60, 1, 2, 50, 411, 4096);
    Serial.println("[SENSOR] MAX30102 ready");

    // BLE init
    setupBLE();

    // Calibration
    delay(2000);
    runCalibration();

    lastSendMs = millis();
}

// MAIN
void loop() {
    long irValue = sensor.getIR();

    // Finger check — skip processing if no contact
    if (irValue < 50000) {
        delay(20);
        return;
    }

    // Use library beat detector instead of manual peak finding on raw buffer.
    // checkForBeat() handles filtering and zero-crossing detection internally,
    // giving cleaner RR intervals for more stable RMSSD.
    if (checkForBeat(irValue)) {
        long now = millis();
        if (lastBeatMs > 0) {
            float rr  = (now - lastBeatMs) / 1000.0f;
            float bpm = 60.0f / rr;
            // 40-180 BPM validity gate (same range as original)
            if (bpm > 40.0f && bpm < 180.0f)
                pushRR(rr);
        }
        lastBeatMs = now;
    }

    // Every 5 minutes: check for stress
    unsigned long now = millis();
    if (calibrated && (now - lastSendMs) >= SEND_INTERVAL_MS) {
        lastSendMs = now;

        float hr, rmssd;
        if (!computeVitalsFromRR(hr, rmssd)) {
            Serial.println("[SKIP] Not enough RR intervals yet");
            delay(20);
            return;
        }

        // Dual threshold check, then get into triple threshold check soon w/ GSR or however we want to do it
        bool hrElevated = (hr - baselineHR) > HR_DELTA_BPM;

        float rmssdDrop    = baselineRMSSD - rmssd;
        float rmssdDropPct = rmssdDrop / baselineRMSSD;
        bool hrvLow = (rmssdDrop > HRV_DELTA_MS) && (rmssdDropPct > HRV_DELTA_PCT);

        Serial.println("\n VITALS ");
        Serial.printf("  HR:    %.1f BPM  (baseline: %.1f)\n", hr, baselineHR);
        Serial.printf("  RMSSD: %.1f ms   (baseline: %.1f)\n", rmssd, baselineRMSSD);
        Serial.printf("  HR elevated: %s | HRV low: %s\n",
                      hrElevated ? "YES" : "no", hrvLow ? "YES" : "no");

        // Only send if BOTH thresholds exceeded
        if (hrElevated && hrvLow) {
            Serial.println("  >> STRESS DETECTED");

            if (deviceConnected) {
                // JSON payload matching Swift app receiving pattern
                char payload[128];
                snprintf(payload, sizeof(payload),
                    "{\"gsr\": %.2f, \"hr\": %.1f, \"hrv\": %.1f, \"stressed\": true}",
                    gsr, hr, rmssd);

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

    // SEND FAKE STRESS EVENT (press 's' in monitor)
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

    delay(20);  // 50 Hz to simulate python computation thats what i had
}
