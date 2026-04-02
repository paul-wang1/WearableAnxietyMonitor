/*
 * ESP32-C3 PPG Stress Monitor
 * 
 * MAX30102 heart rate + HRV stress classifier with BLE output.
 * Only sends BLE notification when BOTH thresholds are exceeded.
 *
 * Thresholds (from papers):
 *   - HR:    +10 BPM above baseline (Juliane Hellhammer conservative vs 19.98 mean)
 *   - RMSSD: -7.9ms AND -17% from baseline (Tarbell et al. 2017) THIS NOT WORKING
 */
#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
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
#define MEASURE_SEC 30 // measurement window for RMSSD reliability THIS NOT WORKING
#define SEND_INTERVAL_MS 300000UL  // check every 5 minutes

#define CALIB_SAMPLES   (SR * CALIB_SEC)
#define MEASURE_SAMPLES (SR * MEASURE_SEC)

// Stress thresholds CHANGE BASED OFF OF PAPERS BUT SEEM GOOD
#define HR_DELTA_BPM 10.0f     // HR must rise by this much
#define HRV_DELTA_MS 7.9f      // RMSSD must drop by this much (absolute)
#define HRV_DELTA_PCT 0.17f     // RMSSD must drop by this much (relative)

// Filter coefficients to match my lab used cheby2
// Chebyshev II bandpass 0.5-5 Hz at fs=50 Hz, 20dB stopband
// scipy.signal.cheby2(2, 20, [0.5, 5.0], btype='band', output='sos', fs=50)
const float SOS[2][6] = {
    { 0.01011f,  0.0f, -0.01011f,  1.0f, -1.75175f,  0.77259f },
    { 1.0f,      0.0f, -1.0f,      1.0f, -1.86427f,  0.89793f }
};

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

// Circular buffer for filtered IR samples
float irBuf[MEASURE_SAMPLES];
int   bufHead  = 0;
int   bufCount = 0;

// IIR filter state
float irState[2][2] = {{0}};

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

float applyBiquad(float x, const float sos[6], float state[2]) {
    float y   = sos[0] * x + state[0];
    state[0]  = sos[1] * x - sos[4] * y + state[1];
    state[1]  = sos[2] * x - sos[5] * y;
    return y;
}

float filterSample(float x) {
    for (int i = 0; i < 2; i++)
        x = applyBiquad(x, SOS[i], irState[i]);
    return x;
}

void computeMeanStd(const float *buf, int len, float &mean, float &stddev) {
    mean = 0.0f;
    for (int i = 0; i < len; i++) mean += buf[i];
    mean /= len;

    float var = 0.0f;
    for (int i = 0; i < len; i++) var += (buf[i] - mean) * (buf[i] - mean);
    stddev = sqrtf(var / len);
}

int findPeaks(const float *sig, int len, int *peakIdx, int maxPeaks) {
    const int minDist = (int)(0.4f * SR);  // 0.4s min between peaks (~150 BPM max)
    int nPeaks = 0;
    int lastPeak = -minDist;

    for (int i = 1; i < len - 1; i++) {
        if (sig[i] > sig[i-1] && sig[i] > sig[i+1]
            && sig[i] > 0.5f
            && (i - lastPeak) >= minDist) {
            if (nPeaks < maxPeaks) {
                peakIdx[nPeaks++] = i;
                lastPeak = i;
            }
        }
    }
    return nPeaks;
}

bool computeVitals(const float *normBuf, int len, float &hr, float &rmssd) {
    hr    = 0.0f;
    rmssd = 0.0f;

    int peaks[200];
    int nPeaks = findPeaks(normBuf, len, peaks, 200);

    if (nPeaks < 3) {
        Serial.println("[WARN] Too few peaks");
        return false;
    }

    // Compute valid RR intervals (40-180 BPM range)
    float rrArr[200];
    int   nRR = 0;
    float rrSum = 0.0f;

    for (int i = 1; i < nPeaks; i++) {
        float rr = (peaks[i] - peaks[i-1]) / (float)SR;
        if (rr >= 0.333f && rr <= 1.5f) {  // 40-180 BPM
            rrArr[nRR++] = rr;
            rrSum += rr;
        }
    }

    if (nRR < 2) return false;

    // HR from mean RR
    float meanRR = rrSum / nRR;
    hr = 60.0f / meanRR;
    if (hr < 40.0f || hr > 180.0f) return false;

    // Outlier rejection (remove RR intervals >20% from median) 
    // Sort a copy to find median
    float rrSorted[200];
    memcpy(rrSorted, rrArr, nRR * sizeof(float));
    for (int i = 0; i < nRR - 1; i++) {
        for (int j = i + 1; j < nRR; j++) {
            if (rrSorted[j] < rrSorted[i]) {
                float tmp = rrSorted[i];
                rrSorted[i] = rrSorted[j];
                rrSorted[j] = tmp;
            }
        }
    }
    float medianRR = rrSorted[nRR / 2];

    // Filter to intervals within 20% of median
    float rrClean[200];
    int nClean = 0;
    for (int i = 0; i < nRR; i++) {
        if (rrArr[i] >= medianRR * 0.80f && rrArr[i] <= medianRR * 1.20f) {
            rrClean[nClean++] = rrArr[i];
        }
    }

    if (nClean < 2) return false;

    // Recalculate HR from cleaned intervals
    float cleanSum = 0.0f;
    for (int i = 0; i < nClean; i++) cleanSum += rrClean[i];
    hr = 60.0f / (cleanSum / nClean);
    
    // THIS IS AN ISSUE
    // RMSSD from successive cleaned RR differences
    float sq = 0.0f;
    for (int i = 1; i < nClean; i++)
        sq += (rrClean[i] - rrClean[i-1]) * (rrClean[i] - rrClean[i-1]);
    rmssd = sqrtf(sq / (nClean - 1)) * 1000.0f;  // ms

    Serial.printf("[DEBUG] RR: %d raw -> %d clean, median: %.0f ms\n", 
                  nRR, nClean, medianRR * 1000);

    return true;
}

bool flattenAndNormalise(float *out, int len) {
    for (int i = 0; i < len; i++)
        out[i] = irBuf[(bufHead + i) % len];

    float mean, stddev;
    computeMeanStd(out, len, mean, stddev);

    if (stddev < 1e-6f) {
        Serial.println("[WARN] Flat signal — check finger placement");
        return false;
    }

    for (int i = 0; i < len; i++)
        out[i] = (out[i] - mean) / stddev;

    return true;
}

// Calibration 1 time at start
void runCalibration(int retryCount = 0) {
    Serial.println("\n CALIBRATION");
    Serial.println("Keep finger on sensor. Collecting baseline for 60 seconds...");

    bufHead  = 0;
    bufCount = 0;
    memset(irState, 0, sizeof(irState));

    for (int s = 0; s < CALIB_SAMPLES; s++) {
        float filtered = filterSample((float)sensor.getIR());
        irBuf[bufHead] = filtered;
        bufHead = (bufHead + 1) % MEASURE_SAMPLES;
        if (bufCount < MEASURE_SAMPLES) bufCount++;

        if (s % (SR * 10) == 0)
            Serial.printf("  %d / %d seconds\n", s / SR, CALIB_SEC);

        delay(20);  // 50 Hz
    }

    float normBuf[MEASURE_SAMPLES];
    if (!flattenAndNormalise(normBuf, MEASURE_SAMPLES)) {
        goto calibFail;
    }

    {
        float hr, rmssd;
        if (!computeVitals(normBuf, MEASURE_SAMPLES, hr, rmssd)) {
            goto calibFail;
        }

        baselineHR    = hr;
        baselineRMSSD = rmssd;
        calibrated    = true;

        Serial.println("CALIBRATION COMPLETE");
        Serial.printf("  Baseline HR:    %.1f BPM\n", baselineHR);
        Serial.printf("  Baseline RMSSD: %.1f ms\n", baselineRMSSD); // this one alwyas tweaking
        Serial.printf("  Stress triggers when: HR > %.1f BPM AND RMSSD < %.1f ms\n",
                      baselineHR + HR_DELTA_BPM,
                      baselineRMSSD - HRV_DELTA_MS);
        return;
    }

calibFail:
    if (retryCount < 1) {
        Serial.println("[CALIB] Failed — retrying in 5s");
        delay(5000);
        runCalibration(retryCount + 1);
    } else {
        Serial.println("[CALIB] Failed twice — halting");
        while (1) delay(1000);
    }
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
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Advertising as 'ESP32C3-StressMonitor'");
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
    // Sample at 50 Hz
    float filtered = filterSample((float)sensor.getIR());
    irBuf[bufHead] = filtered;
    bufHead = (bufHead + 1) % MEASURE_SAMPLES;
    if (bufCount < MEASURE_SAMPLES) bufCount++;

    // Every 5 minutes: check for stress
    unsigned long now = millis();
    if (calibrated && (now - lastSendMs) >= SEND_INTERVAL_MS) {
        lastSendMs = now;

        if (bufCount < MEASURE_SAMPLES) {
            delay(20);
            return;
        }

        float normBuf[MEASURE_SAMPLES];
        if (!flattenAndNormalise(normBuf, MEASURE_SAMPLES)) {
            delay(20);
            return;
        }

        float hr, rmssd;
        if (!computeVitals(normBuf, MEASURE_SAMPLES, hr, rmssd)) {
            Serial.println("[SKIP] Vitals failed");
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
