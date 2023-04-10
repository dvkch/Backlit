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

protocol PreferenceValue: RawRepresentable & CaseIterable & CustomStringConvertible & Equatable {}
extension RawRepresentable where Self: CaseIterable & Equatable {
    var nextValue: Self {
        let allCases = Self.allCases as! [Self]
        let currentIndex = allCases.firstIndex(of: self) ?? -1
        return allCases[(currentIndex + 1) % allCases.count]
    }
}

enum BoolPreferenceValue: PreferenceValue {
    init(rawValue: Bool) {
        self = rawValue ? .on : .off
    }
    
    case on, off

    var rawValue: Bool {
        switch self {
        case .on:  return true
        case .off: return false
        }
    }
    
    var description: String {
        switch self {
        case .on:  return "OPTION BOOL ON".localized
        case .off: return "OPTION BOOL OFF".localized
        }
    }
}

class Preferences: NSObject {
    
    static let shared = Preferences()
    
    private override init() {
        super.init()
        UserDefaults.standard.register(defaults: [userDefaultsKeyImageFormat: ImageFormat.jpeg.rawValue])
        UserDefaults.standard.register(defaults: [userDefaultsKeyShowAdvancedOptions: false])
        UserDefaults.standard.register(defaults: [userDefaultsKeyEnableAnalytics: false])
        UserDefaults.standard.register(defaults: [userDefaultsKeyAskedAnalytics: false])
    }
    
    // MARK: Properties
    enum ImageFormat: String, PreferenceValue {
        case jpeg
        case png
        case heic
        
        var correspondingImageFormat: UIImage.ImageFormat {
            switch self {
            case .jpeg: return .jpeg(quality: 0.9)
            case .png:  return .png
            case .heic: return .heic(quality: 0.9)
            }
        }
        
        var description: String {
            return rawValue.uppercased()
        }
    }
    var imageFormat: ImageFormat {
        get {
            let string = UserDefaults.standard.string(forKey: userDefaultsKeyImageFormat)
            return ImageFormat(rawValue: string ?? "") ?? .jpeg
        }
        set { UserDefaults.standard.set(newValue.rawValue, forKey: userDefaultsKeyImageFormat); postNotification() }
    }
    
    var showAdvancedOptions: Bool {
        get { return UserDefaults.standard.bool(forKey: userDefaultsKeyShowAdvancedOptions) }
        set { UserDefaults.standard.set(newValue, forKey: userDefaultsKeyShowAdvancedOptions); postNotification() }
    }
    
    var previewWithAutoColorMode: Bool {
        get { return Sane.shared.configuration.previewWithAutoColorMode }
        set { Sane.shared.configuration.previewWithAutoColorMode = newValue; postNotification() }
    }
    
    var askedAnalytics: Bool {
        get { return UserDefaults.standard.bool(forKey: userDefaultsKeyAskedAnalytics) }
        set { UserDefaults.standard.set(newValue, forKey: userDefaultsKeyAskedAnalytics); postNotification() }
    }

    var enableAnalytics: Bool {
        get { return UserDefaults.standard.bool(forKey: userDefaultsKeyEnableAnalytics) }
        set { UserDefaults.standard.set(newValue, forKey: userDefaultsKeyEnableAnalytics); askedAnalytics = true; postNotification() }
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
    private let userDefaultsKeyImageFormat          = "ImageFormat"
    private let userDefaultsKeyAskedAnalytics       = "AnalyticsAsked"
    private let userDefaultsKeyEnableAnalytics      = "AnalyticsEnabled"
    private let userDefaultsKeyAnalyticsUserID      = "AnalyticsUserID"
}

// MARK: UI
extension Preferences {
    enum Key {
        case imageFormat, showAdvancedOptions, previewWithAutoColorMode, enableAnalytics
        
        var localizedTitle: String {
            switch self {
            case .imageFormat:              return "PREFERENCES TITLE IMAGE FORMAT".localized
            case .showAdvancedOptions:      return "PREFERENCES TITLE SHOW ADVANCED OPTIONS".localized
            case .previewWithAutoColorMode: return "PREFERENCES TITLE PREVIEW DEFAULT COLOR MODE".localized
            case .enableAnalytics:          return "PREFERENCES TITLE ENABLE ANALYTICS".localized
            }
        }
        
        var localizedDescription: String {
            switch self {
            case .imageFormat:              return "PREFERENCES MESSAGE IMAGE FORMAT".localized
            case .showAdvancedOptions:      return "PREFERENCES MESSAGE SHOW ADVANCED OPTIONS".localized
            case .previewWithAutoColorMode: return "PREFERENCES MESSAGE PREVIEW DEFAULT COLOR MODE".localized
            case .enableAnalytics:          return "PREFERENCES MESSAGE ENABLE ANALYTICS".localized
            }
        }
    }
    
    var groupedKeys: [(String, [Key])] {
        return [
            ("PREFERENCES SECTION PREVIEW".localized, [.previewWithAutoColorMode, .imageFormat]),
            ("PREFERENCES SECTION SCAN".localized, [.showAdvancedOptions]),
            ("PREFERENCES SECTION ANALYTICS".localized, [.enableAnalytics]),
        ]
    }
    
    subscript(key: Key) -> any PreferenceValue {
        get {
            switch key {
            case .imageFormat:              return self.imageFormat
            case .showAdvancedOptions:      return BoolPreferenceValue(rawValue: self.showAdvancedOptions)
            case .previewWithAutoColorMode: return BoolPreferenceValue(rawValue: self.previewWithAutoColorMode)
            case .enableAnalytics:          return BoolPreferenceValue(rawValue: self.enableAnalytics)
            }
        }
        set {
            switch key {
            case .imageFormat:              self.imageFormat = (newValue as? ImageFormat) ?? .jpeg
            case .showAdvancedOptions:      self.showAdvancedOptions = (newValue as? BoolPreferenceValue)?.rawValue == true
            case .previewWithAutoColorMode: self.previewWithAutoColorMode = (newValue as? BoolPreferenceValue)?.rawValue == true
            case .enableAnalytics:          self.enableAnalytics = (newValue as? BoolPreferenceValue)?.rawValue == true
            }
        }
    }
    
    func toggle(key: Key) {
        self[key] = self[key].nextValue
    }
}
