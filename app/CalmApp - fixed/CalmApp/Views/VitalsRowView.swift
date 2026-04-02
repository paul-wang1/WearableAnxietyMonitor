import SwiftUI

struct VitalsRowView: View {
    let event: StressEvent

    var body: some View {
        HStack(spacing: 12) {
            VitalCard(
                icon:  "heart.fill",
                label: "HEART RATE",
                value: "\(Int(event.heartRate))",
                unit:  "bpm",
                color: Color(hex: "#FF6B6B")
            )
            VitalCard(
                icon:  "waveform.path.ecg",
                label: "HRV",
                value: "\(Int(event.hrv))",
                unit:  "ms",
                color: Color(hex: "#64B5F6")
            )
            VitalCard(
                icon:  "drop.fill",
                label: "GSR",
                value: String(format: "%.2f", event.gsrValue),
                unit:  "μS",
                color: Color(hex: "#4FFFB0")
            )
        }
    }
}

struct VitalCard: View {
    let icon:  String
    let label: String
    let value: String
    let unit:  String
    let color: Color

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Image(systemName: icon)
                .foregroundColor(color)
                .font(.system(size: 16))

            Text(label)
                .font(.system(size: 10, weight: .semibold))
                .tracking(1.2)
                .foregroundColor(Color(hex: "#8892A4"))

            HStack(alignment: .lastTextBaseline, spacing: 3) {
                Text(value)
                    .font(.system(size: 20, weight: .semibold))
                    .foregroundColor(Color(hex: "#EEF2FF"))

                Text(unit)
                    .font(.system(size: 12))
                    .foregroundColor(Color(hex: "#8892A4"))
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(16)
        .background(Color(hex: "#111827"))
        .overlay(
            RoundedRectangle(cornerRadius: 16)
                .stroke(Color(hex: "#1E2D42"), lineWidth: 1)
        )
        .clipShape(RoundedRectangle(cornerRadius: 16))
    }
}
