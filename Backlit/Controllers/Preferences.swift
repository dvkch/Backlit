//
//  Preferences.swift
//  Backlit
//
//  Created by syan on 04/02/2019.
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
        case .on:  return L10n.optionBoolOn
        case .off: return L10n.optionBoolOff
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
            case .imageSize: return L10n.preferencesValuePdfSizeImageSize
            case .a4:        return L10n.preferencesValuePdfSizeA4
            case .usLetter:  return L10n.preferencesValuePdfSizeUsLetter
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
            case .imageFormat:              return L10n.preferencesTitleImageFormat
            case .pdfSize:                  return L10n.preferencesTitlePdfSize
            case .showAdvancedOptions:      return L10n.preferencesTitleShowAdvancedOptions
            case .previewWithAutoColorMode: return L10n.preferencesTitlePreviewDefaultColorMode
            case .enableAnalytics:          return L10n.preferencesTitleEnableAnalytics
            }
        }
        
        var localizedDescription: String {
            switch self {
            case .imageFormat:              return L10n.preferencesMessageImageFormat
            case .pdfSize:                  return L10n.preferencesMessagePdfSize
            case .showAdvancedOptions:      return L10n.preferencesMessageShowAdvancedOptions
            case .previewWithAutoColorMode: return L10n.preferencesMessagePreviewDefaultColorMode
            case .enableAnalytics:          return L10n.preferencesMessageEnableAnalytics
            }
        }
    }
    
    var groupedKeys: [(String, [Key])] {
        return [
            (L10n.preferencesSectionPreview, [.previewWithAutoColorMode]),
            (L10n.preferencesSectionScan, [.imageFormat, .pdfSize, .showAdvancedOptions]),
            (L10n.preferencesSectionAnalytics, [.enableAnalytics]),
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
