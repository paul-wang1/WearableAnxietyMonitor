import SwiftUI
import Charts

struct StressTimelineView: View {
    let events: [StressEvent]
    @State private var selectedEvent: StressEvent?

    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 20)
                .fill(Color(hex: "#111827"))
                .overlay(
                    RoundedRectangle(cornerRadius: 20)
                        .stroke(Color(hex: "#1E2D42"), lineWidth: 1)
                )

            if events.isEmpty {
                Text("No stress events today")
                    .font(.system(size: 14))
                    .foregroundColor(Color(hex: "#8892A4"))
                    .padding()
            } else {
                VStack(spacing: 0) {
                    chartView
                        .padding(.horizontal, 12)
                        .padding(.top, 16)
                        .padding(.bottom, selectedEvent != nil ? 8 : 16)
                    
                    if let event = selectedEvent {
                        EventDetailCard(event: event) {
                            withAnimation { selectedEvent = nil }
                        }
                        .transition(.opacity.combined(with: .move(edge: .bottom)))
                        .padding(.horizontal, 12)
                        .padding(.bottom, 12)
                    }
                }
            }
        }
        .frame(height: selectedEvent != nil ? 200 : 120)
        .animation(.easeInOut(duration: 0.2), value: selectedEvent?.id)
    }
    
    private var chartView: some View {
        GeometryReader { geo in
            Chart {
                ForEach(events) { event in
                    let hour = hourValue(for: event.timestamp)

                    PointMark(
                        x: .value("Time", hour),
                        y: .value("Stress", 0.5)
                    )
                    .foregroundStyle(
                        selectedEvent?.id == event.id
                            ? Color(hex: "#FFFFFF")
                            : Color(hex: "#FF6B6B")
                    )
                    .symbolSize(selectedEvent?.id == event.id ? 120 : 80)
                    
                    RuleMark(x: .value("Time", hour))
                        .foregroundStyle(Color(hex: "#FF6B6B").opacity(0.3))
                        .lineStyle(StrokeStyle(lineWidth: 1, dash: [4, 4]))
                }
            }
            .chartXScale(domain: 0...24)
            .chartYScale(domain: 0...1)
            .chartXAxis {
                AxisMarks(values: [0, 6, 12, 18, 24]) { val in
                    AxisValueLabel {
                        if let h = val.as(Int.self) {
                            Text(hourLabel(h))
                                .font(.system(size: 10))
                                .foregroundColor(Color(hex: "#8892A4"))
                        }
                    }
                    AxisGridLine(stroke: StrokeStyle(lineWidth: 0))
                }
            }
            .chartYAxis(.hidden)
            .contentShape(Rectangle())
            .onTapGesture { location in
                handleTap(at: location, in: geo.size)
            }
        }
    }
    
    private func hourValue(for date: Date) -> Double {
        Double(Calendar.current.component(.hour, from: date))
            + Double(Calendar.current.component(.minute, from: date)) / 60.0
    }

    private func hourLabel(_ h: Int) -> String {
        switch h {
        case 0:  return "12a"
        case 12: return "12p"
        case 24: return "12a"
        default: return h < 12 ? "\(h)a" : "\(h - 12)p"
        }
    }
    
    private func handleTap(at location: CGPoint, in size: CGSize) {
        // Convert tap X position to hour (0-24 scale)
        // Account for chart padding (roughly 30pt on left for axis labels)
        let chartPadding: CGFloat = 30
        let chartWidth = size.width - chartPadding
        let tappedHour = Double((location.x - chartPadding) / chartWidth) * 24.0
        
        // Find closest event within 1 hour
        var closest: StressEvent?
        var closestDist = Double.infinity
        
        for event in events {
            let eventHour = hourValue(for: event.timestamp)
            let dist = abs(eventHour - tappedHour)
            if dist < closestDist && dist < 1.5 {
                closest = event
                closestDist = dist
            }
        }
        
        withAnimation {
            selectedEvent = closest
        }
    }
}

struct EventDetailCard: View {
    let event: StressEvent
    let onDismiss: () -> Void
    
    var body: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 2) {
                Text(timeString(event.timestamp))
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundColor(Color(hex: "#FF6B6B"))
                Text("Stress Event")
                    .font(.system(size: 10))
                    .foregroundColor(Color(hex: "#8892A4"))
            }
            
            Spacer()
            
            HStack(spacing: 12) {
                StatPill(label: "HR", value: "\(Int(event.heartRate))", unit: "bpm")
                StatPill(label: "HRV", value: "\(Int(event.hrv))", unit: "ms")
                StatPill(label: "GSR", value: String(format: "%.2f", event.gsrValue), unit: "")
            }
            
            Button(action: onDismiss) {
                Image(systemName: "xmark.circle.fill")
                    .foregroundColor(Color(hex: "#8892A4"))
                    .font(.system(size: 18))
            }
            .buttonStyle(.plain)
        }
        .padding(12)
        .background(Color(hex: "#1E2D42"))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
    
    private func timeString(_ date: Date) -> String {
        let formatter = DateFormatter()
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}

struct StatPill: View {
    let label: String
    let value: String
    let unit: String
    
    var body: some View {
        VStack(spacing: 2) {
            Text(label)
                .font(.system(size: 9, weight: .semibold))
                .foregroundColor(Color(hex: "#8892A4"))
            HStack(spacing: 2) {
                Text(value)
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundColor(Color(hex: "#EEF2FF"))
                if !unit.isEmpty {
                    Text(unit)
                        .font(.system(size: 9))
                        .foregroundColor(Color(hex: "#8892A4"))
                }
            }
        }
    }
}
