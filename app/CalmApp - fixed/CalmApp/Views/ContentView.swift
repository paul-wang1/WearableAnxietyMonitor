import SwiftUI

struct ContentView: View {
    @StateObject private var ble = BLEService()
    @State private var showStressSheet  = false
    @State private var showBreathing    = false
    @State private var triggeredEvent:   StressEvent?

    var body: some View {
        ZStack {
            Color(hex: "#0A0E1A").ignoresSafeArea()

            ScrollView {
                VStack(spacing: 0) {
                    HeaderView(isConnected: ble.isConnected, hasStressEvent: ble.latestEvent != nil)
                        .padding(.horizontal, 24)
                        .padding(.top, 60)

                    StatusOrbView(
                        latestEvent: ble.latestEvent,
                        isConnected: ble.isConnected
                    )
                    .padding(.vertical, 40)

                    // Only show vitals if we have a stress event
                    if let event = ble.latestEvent {
                        VitalsRowView(event: event)
                            .padding(.horizontal, 24)
                    } else if ble.isConnected {
                        // Connected but no stress events yet
                        MonitoringCard()
                            .padding(.horizontal, 24)
                    }

                    // Only show timeline if we have events
                    if !ble.eventHistory.isEmpty {
                        SectionLabel("TODAY'S STRESS EVENTS")
                            .padding(.horizontal, 24)
                            .padding(.top, 40)
                            .padding(.bottom, 12)

                        StressTimelineView(events: ble.eventHistory)
                            .padding(.horizontal, 24)
                    }

                    ConnectButtonView(
                        isConnected: ble.isConnected,
                        isScanning:  ble.isScanning,
                        onConnect:    { ble.startScan() },
                        onDisconnect: { ble.disconnect() }
                    )
                    .padding(.horizontal, 24)
                    .padding(.top, 40)
                    // Clear stress button (only show when stressed) but just switches state
                    if ble.latestEvent != nil {
                        Button {
                            ble.clearStressState()
                        } label: {
                            Text("CLEAR STRESS STATE")
                                .font(.system(size: 11, weight: .semibold))
                                .tracking(1.5)
                                .foregroundColor(Color(hex: "#4FFFB0"))
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 14)
                                .overlay(
                                    RoundedRectangle(cornerRadius: 14)
                                        .stroke(Color(hex: "#4FFFB0").opacity(0.4), lineWidth: 1)
                                )
                        }
                        .buttonStyle(.plain)
                        .padding(.horizontal, 24)
                        .padding(.top, 12)
                                        }
//                    #if DEBUG  // this was to test app but dont need anymore
//                    // Simulate button for testing (only in debug builds)
//                    Button {
//                        ble.injectMockEvent()
//                    } label: {
//                        Text("SIMULATE STRESS EVENT")
//                            .font(.system(size: 11, weight: .semibold))
//                            .tracking(1.5)
//                            .foregroundColor(Color(hex: "#8892A4"))
//                            .frame(maxWidth: .infinity)
//                            .padding(.vertical, 14)
//                            .overlay(
//                                RoundedRectangle(cornerRadius: 14)
//                                    .stroke(Color(hex: "#1E2D42"), lineWidth: 1)
//                            )
//                    }
//                    .buttonStyle(.plain)
//                    .padding(.horizontal, 24)
//                    .padding(.top, 12)
//                    #endif
//                    
//                    Spacer().frame(height: 48)
                }
            }
        }
        .onAppear {
            NotificationService.requestPermission()
            ble.onStressDetected = { event in
                NotificationService.sendStressAlert(
                    heartRate: event.heartRate,
                    hrv: event.hrv
                )
                triggeredEvent = event
                showStressSheet = true
            }
        }
        .sheet(isPresented: $showStressSheet) {
            if let event = triggeredEvent {
                StressPromptSheet(event: event, showBreathing: $showBreathing)
                    .presentationDetents([.medium])
            }
        }
        .fullScreenCover(isPresented: $showBreathing) {
            BreathingView()
        }
    }
}

// MARK: - Section Label
struct SectionLabel: View {
    let text: String
    init(_ text: String) { self.text = text }

    var body: some View {
        HStack {
            Text(text)
                .font(.system(size: 11, weight: .semibold))
                .tracking(1.5)
                .foregroundColor(Color(hex: "#8892A4"))
            Spacer()
        }
    }
}

// MARK: - Monitoring Card (shown when connected but no stress)
struct MonitoringCard: View {
    var body: some View {
        HStack(spacing: 16) {
            ZStack {
                Circle()
                    .fill(Color(hex: "#4FFFB0").opacity(0.15))
                    .frame(width: 44, height: 44)
                Image(systemName: "waveform.path.ecg")
                    .foregroundColor(Color(hex: "#4FFFB0"))
            }
            
            VStack(alignment: .leading, spacing: 4) {
                Text("Actively Monitoring")
                    .font(.system(size: 16, weight: .medium))
                    .foregroundColor(Color(hex: "#EEF2FF"))
                
                Text("Your vitals are being tracked. We'll notify you if stress is detected.")
                    .font(.system(size: 13))
                    .foregroundColor(Color(hex: "#8892A4"))
                    .lineLimit(2)
            }
            
            Spacer()
        }
        .padding(16)
        .background(Color(hex: "#111827"))
        .overlay(
            RoundedRectangle(cornerRadius: 16)
                .stroke(Color(hex: "#1E2D42"), lineWidth: 1)
        )
        .clipShape(RoundedRectangle(cornerRadius: 16))
    }
}
