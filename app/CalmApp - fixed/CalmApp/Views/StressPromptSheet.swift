import SwiftUI

struct StressPromptSheet: View {
    let event: StressEvent
    @Binding var showBreathing: Bool
    @Environment(\.dismiss) var dismiss

    var body: some View {
        ZStack {
            Color(hex: "#1A2235").ignoresSafeArea()

            VStack(alignment: .leading, spacing: 24) {
                HStack(spacing: 14) {
                    ZStack {
                        RoundedRectangle(cornerRadius: 12)
                            .fill(Color(hex: "#FF6B6B").opacity(0.15))
                            .frame(width: 44, height: 44)
                        Image(systemName: "heart.fill")
                            .foregroundColor(Color(hex: "#FF6B6B"))
                    }

                    VStack(alignment: .leading, spacing: 2) {
                        Text("Stress Detected")
                            .font(.system(size: 18, weight: .medium))
                            .foregroundColor(Color(hex: "#EEF2FF"))

                        Text("Your body is showing signs of stress")
                            .font(.system(size: 14))
                            .foregroundColor(Color(hex: "#8892A4"))
                    }
                }

                HStack(spacing: 12) {
                    MiniStatCard(label: "HEART RATE",
                                 value: "\(Int(event.heartRate)) bpm")
                    MiniStatCard(label: "HRV",
                                 value: "\(Int(event.hrv)) ms")
                    MiniStatCard(label: "GSR",
                                 value: String(format: "%.2f", event.gsrValue))
                }

                Button {
                    dismiss()
                    showBreathing = true
                } label: {
                    Text("Start Breathing Exercise")
                        .font(.system(size: 16, weight: .semibold))
                        .foregroundColor(Color(hex: "#0A0E1A"))
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 16)
                        .background(Color(hex: "#4FFFB0"))
                        .clipShape(RoundedRectangle(cornerRadius: 14))
                }
                .buttonStyle(.plain)

                Button {
                    dismiss()
                } label: {
                    Text("Dismiss")
                        .font(.system(size: 14))
                        .foregroundColor(Color(hex: "#8892A4"))
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.plain)
            }
            .padding(28)
        }
    }
}

struct MiniStatCard: View {
    let label: String
    let value: String

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label)
                .font(.system(size: 10, weight: .semibold))
                .tracking(1.2)
                .foregroundColor(Color(hex: "#8892A4"))

            Text(value)
                .font(.system(size: 16, weight: .semibold))
                .foregroundColor(Color(hex: "#EEF2FF"))
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(Color(hex: "#111827"))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}
