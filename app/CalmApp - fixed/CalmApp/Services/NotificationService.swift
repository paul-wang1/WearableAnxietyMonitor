import Foundation
import UserNotifications

class NotificationService {
    static func requestPermission() {
        UNUserNotificationCenter.current().requestAuthorization(
            options: [.alert, .sound, .badge]
        ) { granted, _ in
            print("Notification permission: \(granted)")
        }
    }

    static func sendStressAlert(heartRate: Double, hrv: Double) {
        let content = UNMutableNotificationContent()
        content.title = "Stress Detected"
        content.body  = "HR \(Int(heartRate)) bpm · HRV \(Int(hrv)) ms - Try a breathing exercise"
        content.sound = .default

        let request = UNNotificationRequest(
            identifier: UUID().uuidString,
            content: content,
            trigger: nil  // deliver immediately
        )
        UNUserNotificationCenter.current().add(request)
    }
}
