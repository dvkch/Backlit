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
        let currentIndex = allValues.firstIndex(of: self) ?? -1
        return allValues[(currentIndex + 1) % allValues.count]
    }
    
    var allValues: [Self] {
        return Self.allCases as! [Self]
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
        UserDefaults.standard.register(defaults: [Preferences.Key.imageFormat.rawValue: ImageFormat.jpeg.rawValue])
        UserDefaults.standard.register(defaults: [Preferences.Key.pdfSize.rawValue: PDFSize.imageSize.rawValue])
        UserDefaults.standard.register(defaults: [Preferences.Key.showAdvancedOptions.rawValue: false])
        UserDefaults.standard.register(defaults: [Preferences.Key.enableAnalytics.rawValue: false])
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
            let string = UserDefaults.standard.string(forKey: Preferences.Key.imageFormat.rawValue)
            return ImageFormat(rawValue: string ?? "") ?? .jpeg
        }
        set { UserDefaults.standard.set(newValue.rawValue, forKey: Preferences.Key.imageFormat.rawValue); postNotification() }
    }
    
    enum PDFSize: String, PreferenceValue {
        case imageSize
        case a4
        case usLetter
        
        var description: String {
            switch self {
            case .imageSize: return "PREFERENCES VALUE PDF SIZE IMAGE SIZE".localized
            case .a4:        return "PREFERENCES VALUE PDF SIZE A4".localized
            case .usLetter:  return "PREFERENCES VALUE PDF SIZE US LETTER".localized
            }
        }
    }
    var pdfSize: PDFSize {
        get {
            let string = UserDefaults.standard.string(forKey: Preferences.Key.pdfSize.rawValue)
            return PDFSize(rawValue: string ?? "") ?? .imageSize
        }
        set { UserDefaults.standard.set(newValue.rawValue, forKey: Preferences.Key.pdfSize.rawValue); postNotification() }
    }

    var showAdvancedOptions: Bool {
        get { return UserDefaults.standard.bool(forKey: Preferences.Key.showAdvancedOptions.rawValue) }
        set { UserDefaults.standard.set(newValue, forKey: Preferences.Key.showAdvancedOptions.rawValue); postNotification() }
    }
    
    var previewWithAutoColorMode: Bool {
        get { return Sane.shared.configuration.previewWithAutoColorMode }
        set { Sane.shared.configuration.previewWithAutoColorMode = newValue; postNotification() }
    }
    
    private let userDefaultsKeyAskedAnalytics = "AnalyticsAsked"
    var askedAnalytics: Bool {
        get { return UserDefaults.standard.bool(forKey: userDefaultsKeyAskedAnalytics) }
        set { UserDefaults.standard.set(newValue, forKey: userDefaultsKeyAskedAnalytics); postNotification() }
    }

    var enableAnalytics: Bool {
        get { return UserDefaults.standard.bool(forKey: Preferences.Key.enableAnalytics.rawValue) }
        set { UserDefaults.standard.set(newValue, forKey: Preferences.Key.enableAnalytics.rawValue); askedAnalytics = true; postNotification() }
    }
    
    private let userDefaultsKeyAnalyticsUserID = "AnalyticsUserID"
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
}

// MARK: UI
extension Preferences {
    enum Key: String {
        case imageFormat = "ImageFormat"
        case pdfSize = "PDFSize"
        case showAdvancedOptions = "ShowAdvancedOptions"
        case previewWithAutoColorMode = "sane-previewWithAutoColorMode"
        case enableAnalytics = "AnalyticsEnabled"
        
        var localizedTitle: String {
            switch self {
            case .imageFormat:              return "PREFERENCES TITLE IMAGE FORMAT".localized
            case .pdfSize:                  return "PREFERENCES TITLE PDF SIZE".localized
            case .showAdvancedOptions:      return "PREFERENCES TITLE SHOW ADVANCED OPTIONS".localized
            case .previewWithAutoColorMode: return "PREFERENCES TITLE PREVIEW DEFAULT COLOR MODE".localized
            case .enableAnalytics:          return "PREFERENCES TITLE ENABLE ANALYTICS".localized
            }
        }
        
        var localizedDescription: String {
            switch self {
            case .imageFormat:              return "PREFERENCES MESSAGE IMAGE FORMAT".localized
            case .pdfSize:                  return "PREFERENCES MESSAGE PDF SIZE".localized
            case .showAdvancedOptions:      return "PREFERENCES MESSAGE SHOW ADVANCED OPTIONS".localized
            case .previewWithAutoColorMode: return "PREFERENCES MESSAGE PREVIEW DEFAULT COLOR MODE".localized
            case .enableAnalytics:          return "PREFERENCES MESSAGE ENABLE ANALYTICS".localized
            }
        }
    }
    
    var groupedKeys: [(String, [Key])] {
        return [
            ("PREFERENCES SECTION PREVIEW".localized, [.previewWithAutoColorMode]),
            ("PREFERENCES SECTION SCAN".localized, [.imageFormat, .pdfSize, .showAdvancedOptions]),
            ("PREFERENCES SECTION ANALYTICS".localized, [.enableAnalytics]),
        ]
    }
    
    subscript(key: Key) -> any PreferenceValue {
        get {
            switch key {
            case .imageFormat:              return self.imageFormat
            case .pdfSize:                  return self.pdfSize
            case .showAdvancedOptions:      return BoolPreferenceValue(rawValue: self.showAdvancedOptions)
            case .previewWithAutoColorMode: return BoolPreferenceValue(rawValue: self.previewWithAutoColorMode)
            case .enableAnalytics:          return BoolPreferenceValue(rawValue: self.enableAnalytics)
            }
        }
        set {
            switch key {
            case .imageFormat:              self.imageFormat = (newValue as? ImageFormat) ?? .jpeg
            case .pdfSize:                  self.pdfSize = (newValue as? PDFSize) ?? .imageSize
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
