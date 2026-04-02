import Foundation
import CoreBluetooth
import Combine

// Making sure it matches esp32
let kServiceUUID        = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
let kCharacteristicUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")

class BLEService: NSObject, ObservableObject {
    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var resetTimer: Timer?
    
    // How long until stress state auto resets 300 is 5 mins
    private let stressResetInterval: TimeInterval = 300

    @Published var isConnected   = false
    @Published var isScanning    = false
    @Published var latestEvent:   StressEvent?
    @Published var eventHistory: [StressEvent] = []
    @Published var statusMessage = "Not connected"

    var onStressDetected: ((StressEvent) -> Void)?

    override init() {
        super.init()
        // Enable background BLE to restore
        centralManager = CBCentralManager(
            delegate: self,
            queue: nil,
            options: [CBCentralManagerOptionRestoreIdentifierKey: "CalmAppBLE"]
        )
    }

    func startScan() {
        guard centralManager.state == .poweredOn else {
            statusMessage = "Bluetooth is off"
            return
        }
        isScanning = true
        statusMessage = "Scanning..."
        centralManager.scanForPeripherals(withServices: nil, options: nil)

        // Timeout after 10s
        DispatchQueue.main.asyncAfter(deadline: .now() + 10) { [weak self] in
            guard let self, self.isScanning else { return }
            self.stopScan()
            self.statusMessage = "Device not found"
        }
    }

    func stopScan() {
        centralManager.stopScan()
        isScanning = false
    }

    func disconnect() {
        guard let p = peripheral else { return }
        centralManager.cancelPeripheralConnection(p)
    }

    // MARK: - Mock / testing
    func injectMockEvent() {
        let event = StressEvent(
            timestamp: Date(),
            gsrValue:  0.65,
            heartRate: 92,
            hrv:       28,
            level:     .high
        )
        publish(event)
    }
    
    // MARK: - Reset stress state
    func clearStressState() {
        DispatchQueue.main.async {
            self.latestEvent = nil
            self.statusMessage = "Monitoring"
        }
    }
    
    private func startResetTimer() {
        // Cancel existing timer
        resetTimer?.invalidate()
        
        // Start new 5 minute timer
        resetTimer = Timer.scheduledTimer(withTimeInterval: stressResetInterval, repeats: false) { [weak self] _ in
            guard let self else { return }
            print("[BLE] 5 minutes passed — clearing stress state")
            self.clearStressState()
        }
    }

    // MARK: - Private
    private func publish(_ event: StressEvent) {
        DispatchQueue.main.async { [weak self] in
            guard let self else { return }
            self.latestEvent = event
            self.eventHistory.append(event)
            
            // Keep only last 24h
            let cutoff = Date().addingTimeInterval(-86400)
            self.eventHistory.removeAll { $0.timestamp < cutoff }
            
            // Start/restart the 5 minute reset timer
            self.startResetTimer()
            
            // Notify (triggers notification even in background)
            self.onStressDetected?(event)
        }
    }
}

// MARK: - CBCentralManagerDelegate
extension BLEService: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            statusMessage = "Ready"
        } else {
            statusMessage = "Bluetooth unavailable"
        }
    }
    
    // Called when app restores from background
    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        if let peripherals = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral],
           let restored = peripherals.first {
            peripheral = restored
            peripheral?.delegate = self
            isConnected = true
            statusMessage = "Connected"
            print("[BLE] Restored connection from background")
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        let name = peripheral.name ?? ""
        print("Found: \(name)")
        guard name.hasPrefix("ESP32C3") else { return }

        stopScan()
        self.peripheral = peripheral
        self.peripheral?.delegate = self
        central.connect(peripheral, options: nil)
        statusMessage = "Connecting..."
    }

    func centralManager(_ central: CBCentralManager,
                        didConnect peripheral: CBPeripheral) {
        isConnected  = true
        statusMessage = "Connected"
        peripheral.discoverServices([kServiceUUID])
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        isConnected   = false
        self.peripheral = nil
        statusMessage  = "Disconnected"
        
        // Auto-reconnect if disconnected unexpectedly
        if error != nil {
            startScan()
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        statusMessage = "Failed to connect"
        isScanning    = false
    }
}

// MARK: - CBPeripheralDelegate
extension BLEService: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            if service.uuid == kServiceUUID {
                peripheral.discoverCharacteristics([kCharacteristicUUID], for: service)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let chars = service.characteristics else { return }
        for char in chars {
            if char.uuid == kCharacteristicUUID {
                peripheral.setNotifyValue(true, for: char)
                print("Subscribed to stress characteristic")
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard let data = characteristic.value,
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Double],
              let gsr  = json["gsr"],
              let hr   = json["hr"],
              let hrv  = json["hrv"]
        else { return }

        let event = StressEvent(
            timestamp: Date(),
            gsrValue:  gsr,
            heartRate: hr,
            hrv:       hrv,
            level:     .high
        )
        publish(event)
    }
}
