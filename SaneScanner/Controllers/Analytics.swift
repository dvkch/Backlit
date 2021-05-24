//
//  Analytics.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation
import TelemetryClient
import SaneSwift
import UIKit

class Analytics {

    // MARK: Init
    static let shared = Analytics()
    private init() {
        let config = TelemetryManagerConfiguration.init(appID: "9CF71A71-190A-4B84-AB6B-2E0DE0A44F12")
        config.telemetryAllowDebugBuilds = true
        TelemetryManager.initialize(with: config)
    }
    
    // MARK: Events
    enum Event {
        case appLaunch
        case newHostTapped
        case newHostAdded(count: Int)
        case scanStarted(device: Device)
        case scanEnded(device: Device)
        case scanCancelled(device: Device)
        case scanFailed(device: Device, error: Error)
        case previewStarted(device: Device)
        case previewEnded(device: Device)
        case previewCancelled(device: Device)
        case previewFailed(device: Device, error: Error)

        var name: String {
            switch self {
            case .appLaunch:        return "App Launch"
            case .newHostTapped:    return "New Host Tap"
            case .newHostAdded:     return "New Host Added"
            case .scanStarted:      return "Scan Started"
            case .scanEnded:        return "Scan Ended"
            case .scanCancelled:    return "Scan Cancelled"
            case .scanFailed:       return "Scan Failed"
            case .previewStarted:    return "Preview Started"
            case .previewEnded:      return "Preview Ended"
            case .previewCancelled:  return "Preview Cancelled"
            case .previewFailed:     return "Preview Failed"
            }
        }
        
        var data: [String: String] {
            switch self {
            case .appLaunch:                            return [:]
            case .newHostTapped:                        return [:]
            case .newHostAdded(let count):              return ["Hosts Count": String(count)]
            case .scanStarted(let device):              return ["Device Maker": device.vendor]
            case .scanEnded(let device):                return ["Device Maker": device.vendor]
            case .scanCancelled(let device):            return ["Device Maker": device.vendor]
            case .scanFailed(let device, let error):    return ["Device Maker": device.vendor, "Error": error.localizedDescription]
            case .previewStarted(let device):           return ["Device Maker": device.vendor]
            case .previewEnded(let device):             return ["Device Maker": device.vendor]
            case .previewCancelled(let device):         return ["Device Maker": device.vendor]
            case .previewFailed(let device, let error): return ["Device Maker": device.vendor, "Error": error.localizedDescription]
            }
        }
    }
    
    func send(event: Event) {
        guard Preferences.shared.enableAnalytics else { return }
        TelemetryManager.send(event.name, for: Preferences.shared.analyticsUserID, with: event.data)
    }
}
