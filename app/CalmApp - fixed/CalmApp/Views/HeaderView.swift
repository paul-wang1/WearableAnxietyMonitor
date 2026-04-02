import SwiftUI

struct HeaderView: View {
    let isConnected: Bool
    let hasStressEvent: Bool

    private var greeting: String {
        let hour = Calendar.current.component(.hour, from: Date())
        if hour < 12 { return "Good morning" }
        if hour < 17 { return "Good afternoon" }
        return "Good evening"
    }

    var body: some View {
        HStack(alignment: .top) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Calm Collective Michael, Jacky and Paul")
                    .font(.system(size: 36, weight: .ultraLight))
                    .tracking(-2)
                    .foregroundColor(Color(hex: "#4FFFB0"))

                Text(greeting)
                    .font(.system(size: 14))
                    .foregroundColor(Color(hex: "#8892A4"))
            }

            Spacer()

            StatusBadge(isConnected: isConnected, hasStressEvent: hasStressEvent)
        }
    }
}

struct StatusBadge: View {
    let isConnected: Bool
    let hasStressEvent: Bool
    
    private var badgeText: String {
        if !isConnected { return "OFFLINE" }
        if hasStressEvent { return "STRESSED" }
        return "MONITORING"
    }
    
    private var badgeColor: Color {
        if !isConnected { return Color(hex: "#8892A4") }
        if hasStressEvent { return Color(hex: "#FF6B6B") }
        return Color(hex: "#4FFFB0")
    }

    var body: some View {
        HStack(spacing: 7) {
            Circle()
                .fill(badgeColor)
                .frame(width: 7, height: 7)

            Text(badgeText)
                .font(.system(size: 11, weight: .semibold))
                .tracking(1.2)
                .foregroundColor(badgeColor)
        }
        .padding(.horizontal, 14)
        .padding(.vertical, 8)
        .background(badgeColor.opacity(0.12))
        .overlay(
            RoundedRectangle(cornerRadius: 20)
                .stroke(badgeColor.opacity(0.3), lineWidth: 1)
        )
        .clipShape(RoundedRectangle(cornerRadius: 20))
    }
}
