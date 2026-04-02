import SwiftUI

struct StatusOrbView: View {
    let latestEvent: StressEvent?
    let isConnected: Bool

    @State private var pulse = false
    @State private var glow  = false

    private var orbColor: Color {
        if !isConnected {
            return Color(hex: "#8892A4")
        }
        if let event = latestEvent {
            return event.level.color
        }
        return Color(hex: "#4FFFB0") // Monitoring state like red ish
    }

    private var label: String {
        if !isConnected { return "Not Connected" }
        if latestEvent != nil {
            return "Stress Detected"
        }
        return "Monitoring"
    }

    private var sublabel: String {
        if !isConnected {
            return "Tap connect to pair your device"
        }
        if let event = latestEvent {
            return "Detected at \(timeString(event.timestamp))"
        }
        return "We'll alert you if stress is detected"
    }
    
    private var iconName: String {
        if !isConnected { return "bluetooth" }
        if latestEvent != nil { return "heart.fill" }
        return "waveform.path.ecg"
    }

    var body: some View {
        VStack(spacing: 28) {
            ZStack {
                // Outer glow
                Circle()
                    .fill(orbColor.opacity(glow ? 0.15 : 0.05))
                    .frame(width: 220, height: 220)
                    .blur(radius: 20)
                    .scaleEffect(pulse ? 1.08 : 1.0)

                // Main orb
                Circle()
                    .fill(
                        RadialGradient(
                            colors: [orbColor.opacity(0.5), orbColor.opacity(0.05)],
                            center: .center,
                            startRadius: 0,
                            endRadius: 80
                        )
                    )
                    .frame(width: 160, height: 160)
                    .overlay(
                        Circle()
                            .stroke(orbColor.opacity(0.4), lineWidth: 1.5)
                    )
                    .shadow(color: orbColor.opacity(glow ? 0.5 : 0.2), radius: 40)
                    .scaleEffect(pulse ? 1.04 : 1.0)

                // Inner icon
                ZStack {
                    Circle()
                        .fill(orbColor.opacity(0.3))
                        .frame(width: 50, height: 50)

                    Image(systemName: iconName)
                        .foregroundColor(orbColor)
                        .font(.system(size: 22))
                }
            }
            .onAppear {
                withAnimation(.easeInOut(duration: 3).repeatForever(autoreverses: true)) {
                    pulse = true
                    glow  = true
                }
            }

            VStack(spacing: 8) {
                Text(label)
                    .font(.system(size: 28, weight: .light))
                    .tracking(-0.5)
                    .foregroundColor(orbColor)
                    .animation(.easeInOut(duration: 0.4), value: label)

                Text(sublabel)
                    .font(.system(size: 14))
                    .foregroundColor(Color(hex: "#8892A4"))
                    .multilineTextAlignment(.center)
            }
        }
    }
    
    private func timeString(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}
