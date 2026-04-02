import SwiftUI

struct ConnectButtonView: View {
    let isConnected:  Bool
    let isScanning:   Bool
    let onConnect:    () -> Void
    let onDisconnect: () -> Void

    var body: some View {
        if isConnected {
            Button(action: onDisconnect) {
                HStack {
                    Image(systemName: "bluetooth")
                        .foregroundColor(Color(hex: "#4FFFB0"))
                    Text("Connected · Tap to disconnect")
                        .font(.system(size: 14))
                        .foregroundColor(Color(hex: "#4FFFB0"))
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 16)
                .overlay(
                    RoundedRectangle(cornerRadius: 14)
                        .stroke(Color(hex: "#4FFFB0").opacity(0.4), lineWidth: 1)
                )
            }
            .buttonStyle(.plain)
        } else {
            Button(action: onConnect) {
                HStack(spacing: 10) {
                    if isScanning {
                        ProgressView()
                            .progressViewStyle(CircularProgressViewStyle(tint: Color(hex: "#0A0E1A")))
                            .scaleEffect(0.8)
                    } else {
                        Image(systemName: "bluetooth.circle.fill")
                    }
                    Text(isScanning ? "Scanning..." : "Connect to Device")
                        .font(.system(size: 15, weight: .semibold))
                }
                .foregroundColor(Color(hex: "#0A0E1A"))
                .frame(maxWidth: .infinity)
                .padding(.vertical, 16)
                .background(Color(hex: "#4FFFB0"))
                .clipShape(RoundedRectangle(cornerRadius: 14))
            }
            .buttonStyle(.plain)
            .disabled(isScanning)
            .opacity(isScanning ? 0.7 : 1.0)
        }
    }
}
