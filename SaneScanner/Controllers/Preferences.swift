//
//  Preferences.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift

extension Notification.Name {
    static let preferencesChanged = Notification.Name("SYPreferencesChangedNotification")
}

class Preferences: NSObject {
    
    static let shared = Preferences()
    
    private override init() {
        super.init()
        UserDefaults.standard.register(defaults: [userDefaultsKeySaveAsPNG: false])
        UserDefaults.standard.register(defaults: [userDefaultsKeyShowAdvancedOptions: false])
        UserDefaults.standard.register(defaults: [userDefaultsKeyEnableAnalytics: false])
    }
    
    // MARK: Properties
    var saveAsPNG: Bool {
        get { return UserDefaults.standard.bool(forKey: userDefaultsKeySaveAsPNG) }
        set { UserDefaults.standard.set(newValue, forKey: userDefaultsKeySaveAsPNG); postNotification() }
    }
    
    var showAdvancedOptions: Bool {
        get { return UserDefaults.standard.bool(forKey: userDefaultsKeyShowAdvancedOptions) }
        set { UserDefaults.standard.set(newValue, forKey: userDefaultsKeyShowAdvancedOptions); postNotification() }
    }
    
    var previewWithAutoColorMode: Bool {
        get { return Sane.shared.configuration.previewWithAutoColorMode }
        set { Sane.shared.configuration.previewWithAutoColorMode = newValue; postNotification() }
    }
    
    var enableAnalytics: Bool {
        get { return UserDefaults.standard.bool(forKey: userDefaultsKeyEnableAnalytics) }
        set { UserDefaults.standard.set(newValue, forKey: userDefaultsKeyEnableAnalytics); postNotification() }
    }
    
    var analyticsUserID: String {
        get {
            if let id = UserDefaults.standard.string(forKey: userDefaultsKeyAnalyticsUserID) {
                return id
            }
            let id = NSUUID().uuidString
            UserDefaults.standard.set(id, forKey: userDefaultsKeyAnalyticsUserID)
            return id
        }
    }
    
    // MARK: Notification
    private func postNotification() {
        DispatchQueue.main.async {
            NotificationCenter.default.post(name: .preferencesChanged, object: self)
        }
    }
    
    // MARK: UserDefaults keys
    private let userDefaultsKeyShowAdvancedOptions  = "ShowAdvancedOptions"
    private let userDefaultsKeySaveAsPNG            = "SaveAsPNG"
    private let userDefaultsKeyEnableAnalytics      = "AnalyticsEnabled"
    private let userDefaultsKeyAnalyticsUserID      = "AnalyticsUserID"
}

// MARK: UI
extension Preferences {
    enum Key {
        case saveAsPNG, showAdvancedOptions, previewWithAutoColorMode, enableAnalytics
        
        var localizedTitle: String {
            switch self {
            case .saveAsPNG:                return "PREFERENCES TITLE SAVE AS PNG".localized
            case .showAdvancedOptions:      return "PREFERENCES TITLE SHOW ADVANCED OPTIONS".localized
            case .previewWithAutoColorMode: return "PREFERENCES TITLE PREVIEW DEFAULT COLOR MODE".localized
            case .enableAnalytics:          return "PREFERENCES TITLE ENABLE ANALYTICS".localized
            }
        }
        
        var localizedDescription: String {
            switch self {
            case .saveAsPNG:                return "PREFERENCES MESSAGE SAVE AS PNG".localized
            case .showAdvancedOptions:      return "PREFERENCES MESSAGE SHOW ADVANCED OPTIONS".localized
            case .previewWithAutoColorMode: return "PREFERENCES MESSAGE PREVIEW DEFAULT COLOR MODE".localized
            case .enableAnalytics:          return "PREFERENCES MESSAGE ENABLE ANALYTICS".localized
            }
        }
    }
    
    var groupedKeys: [(String, [Key])] {
        return [
            ("PREFERENCES SECTION PREVIEW".localized, [.previewWithAutoColorMode, .saveAsPNG]),
            ("PREFERENCES SECTION SCAN".localized, [.showAdvancedOptions]),
            ("PREFERENCES SECTION ANALYTICS".localized, [.enableAnalytics]),
        ]
    }
    
    subscript(key: Key) -> Bool {
        get {
            switch key {
            case .saveAsPNG:                return self.saveAsPNG
            case .showAdvancedOptions:      return self.showAdvancedOptions
            case .previewWithAutoColorMode: return self.previewWithAutoColorMode
            case .enableAnalytics:          return self.enableAnalytics
            }
        }
        set {
            switch key {
            case .saveAsPNG:                self.saveAsPNG = newValue
            case .showAdvancedOptions:      self.showAdvancedOptions = newValue
            case .previewWithAutoColorMode: self.previewWithAutoColorMode = newValue
            case .enableAnalytics:          self.enableAnalytics = newValue
            }
        }
    }
    
    func toggle(key: Key) {
        self[key] = !self[key]
    }
}
