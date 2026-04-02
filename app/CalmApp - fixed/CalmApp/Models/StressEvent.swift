import Foundation
import SwiftUI

enum StressLevel: String {
    case monitoring = "Monitoring"
    case high       = "Stressed"

    var color: Color {
        switch self {
        case .monitoring: return Color(hex: "#4FFFB0")
        case .high:       return Color(hex: "#FF6B6B")
        }
    }

    var normalizedValue: Double {
        switch self {
        case .monitoring: return 0.15
        case .high:       return 0.90
        }
    }

    var icon: String {
        switch self {
        case .monitoring: return "checkmark.circle.fill"
        case .high:       return "heart.fill"
        }
    }

    var sublabel: String {
        switch self {
        case .monitoring: return "We'll alert you if stress is detected"
        case .high:       return "Try the breathing exercise below"
        }
    }
}

struct StressEvent: Identifiable {
    let id = UUID()
    let timestamp: Date
    let gsrValue: Double
    let heartRate: Double
    let hrv: Double
    let level: StressLevel
}

// MARK: - Color hex init
extension Color {
    init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var int: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&int)
        let r = Double((int >> 16) & 0xFF) / 255
        let g = Double((int >> 8)  & 0xFF) / 255
        let b = Double(int & 0xFF)          / 255
        self.init(red: r, green: g, blue: b)
    }
}
