import SwiftUI

private enum BreathPhase: String {
    case inhale  = "Breathe In"
    case hold    = "Hold"
    case exhale  = "Breathe Out"
    case rest    = "Rest"

    var duration: Int {
        switch self {
        case .inhale:  return 4
        case .hold:    return 7
        case .exhale:  return 8
        case .rest:    return 1
        }
    }

    var next: BreathPhase {
        switch self {
        case .inhale:  return .hold
        case .hold:    return .exhale
        case .exhale:  return .rest
        case .rest:    return .inhale
        }
    }
}

struct BreathingView: View {
    @Environment(\.dismiss) var dismiss

    @State private var phase:      BreathPhase = .inhale
    @State private var countdown:  Int         = 4
    @State private var cycleCount: Int         = 0
    @State private var orbScale:   CGFloat     = 0.55
    @State private var glowOpacity: Double     = 0.3
    @State private var done        = false

    private let totalCycles = 4
    private let timer = Timer.publish(every: 1, on: .main, in: .common).autoconnect()

    var body: some View {
        ZStack {
            Color(hex: "#0A0E1A").ignoresSafeArea()

            if done {
                doneView
            } else {
                breathingView
            }
        }
        .onReceive(timer) { _ in
            guard !done else { return }
            tick()
        }
        .onAppear {
            startPhase(.inhale)
        }
    }

    // MARK: - Breathing view
    private var breathingView: some View {
        VStack(spacing: 0) {
            // Top bar
            HStack {
                Button { dismiss() } label: {
                    Image(systemName: "xmark")
                        .foregroundColor(Color(hex: "#8892A4"))
                        .font(.system(size: 16))
                }
                .buttonStyle(.plain)

                Spacer()

                Text("4 · 7 · 8  BREATHING")
                    .font(.system(size: 11, weight: .semibold))
                    .tracking(1.5)
                    .foregroundColor(Color(hex: "#8892A4"))

                Spacer()
                Color.clear.frame(width: 16)
            }
            .padding(.horizontal, 24)
            .padding(.top, 60)

            Spacer()

            Text("Cycle \(cycleCount + 1) of \(totalCycles)")
                .font(.system(size: 14))
                .foregroundColor(Color(hex: "#8892A4"))

            Spacer().frame(height: 48)

            // Orb
            ZStack {
                Circle()
                    .fill(Color(hex: "#4FFFB0").opacity(glowOpacity * 0.3))
                    .frame(width: 280, height: 280)
                    .blur(radius: 40)
                    .scaleEffect(orbScale * 1.2)

                Circle()
                    .fill(
                        RadialGradient(
                            colors: [
                                Color(hex: "#4FFFB0").opacity(0.6),
                                Color(hex: "#4FFFB0").opacity(0.1)
                            ],
                            center: .center,
                            startRadius: 0,
                            endRadius: 90
                        )
                    )
                    .frame(width: 180, height: 180)
                    .overlay(
                        Circle()
                            .stroke(Color(hex: "#4FFFB0").opacity(0.6), lineWidth: 1.5)
                    )
                    .shadow(color: Color(hex: "#4FFFB0").opacity(glowOpacity), radius: 40)
                    .scaleEffect(orbScale)
            }
            .animation(.easeInOut(duration: Double(phase.duration)), value: orbScale)

            Spacer().frame(height: 48)

            Text(phase.rawValue)
                .font(.system(size: 32, weight: .light))
                .tracking(-0.5)
                .foregroundColor(Color(hex: "#EEF2FF"))

            Spacer().frame(height: 16)

            Text("\(countdown)")
                .font(.system(size: 64, weight: .thin))
                .foregroundColor(Color(hex: "#4FFFB0"))
                .id(countdown)
                .transition(.opacity)
                .animation(.easeInOut(duration: 0.3), value: countdown)

            Spacer().frame(height: 60)

            Text("Focus on your breath.\n Think of something else to say here.")
                .font(.system(size: 14))
                .foregroundColor(Color(hex: "#8892A4"))
                .multilineTextAlignment(.center)
                .lineSpacing(4)
                .padding(.horizontal, 48)

            Spacer()
        }
    }

    // MARK: - Done view
    private var doneView: some View {
        VStack(spacing: 32) {
            ZStack {
                Circle()
                    .fill(Color(hex: "#4FFFB0").opacity(0.12))
                    .frame(width: 96, height: 96)
                Image(systemName: "checkmark")
                    .font(.system(size: 40, weight: .light))
                    .foregroundColor(Color(hex: "#4FFFB0"))
            }

            VStack(spacing: 12) {
                Text("Well done.")
                    .font(.system(size: 32, weight: .light))
                    .foregroundColor(Color(hex: "#EEF2FF"))

                Text("You completed \(totalCycles) breath cycles.\n Great job!")
                    .font(.system(size: 16))
                    .foregroundColor(Color(hex: "#8892A4"))
                    .multilineTextAlignment(.center)
                    .lineSpacing(4)
            }

            Button { dismiss() } label: {
                Text("Back to Dashboard")
                    .font(.system(size: 16, weight: .semibold))
                    .foregroundColor(Color(hex: "#0A0E1A"))
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 16)
                    .background(Color(hex: "#4FFFB0"))
                    .clipShape(RoundedRectangle(cornerRadius: 14))
            }
            .buttonStyle(.plain)
            .padding(.horizontal, 40)
        }
        .padding(40)
    }

    // MARK: - Logic
    private func tick() {
        if countdown > 1 {
            countdown -= 1
        } else {
            advancePhase()
        }
    }

    private func advancePhase() {
        if phase == .exhale {
            cycleCount += 1
            if cycleCount >= totalCycles {
                done = true
                return
            }
        }
        startPhase(phase.next)
    }

    private func startPhase(_ p: BreathPhase) {
        phase     = p
        countdown = p.duration

        switch p {
        case .inhale:
            orbScale     = 1.0
            glowOpacity  = 0.6
        case .hold:
            break
        case .exhale:
            orbScale     = 0.55
            glowOpacity  = 0.2
        case .rest:
            break
        }
    }
}
