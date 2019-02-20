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
            
        UserDefaults.standard.register(defaults: [useDefaultsKeySaveAsPNG: false])
        UserDefaults.standard.register(defaults: [useDefaultsKeyShowAdvancedOptions: false])
    }
    
    // MARK: Properties
    var saveAsPNG: Bool {
        get { return UserDefaults.standard.bool(forKey: useDefaultsKeySaveAsPNG) }
        set { UserDefaults.standard.set(newValue, forKey: useDefaultsKeySaveAsPNG); postNotification() }
    }
    
    var showAdvancedOptions: Bool {
        get { return UserDefaults.standard.bool(forKey: useDefaultsKeyShowAdvancedOptions) }
        set { UserDefaults.standard.set(newValue, forKey: useDefaultsKeyShowAdvancedOptions); postNotification() }
    }
    
    var showIncompleteScanImages: Bool {
        get { return Sane.shared.configuration.showIncompleteScanImages }
        set { Sane.shared.configuration.showIncompleteScanImages = newValue; postNotification() }
    }
    
    var previewWithAutoColorMode: Bool {
        get { return Sane.shared.configuration.previewWithAutoColorMode }
        set { Sane.shared.configuration.previewWithAutoColorMode = newValue; postNotification() }
    }
    
    // MARK: Notification
    private func postNotification() {
        DispatchQueue.main.async {
            NotificationCenter.default.post(name: .preferencesChanged, object: self)
        }
    }
    
    // MARK: UserDefaults keys
    private let useDefaultsKeyShowAdvancedOptions   = "ShowAdvancedOptions"
    private let useDefaultsKeySaveAsPNG             = "SaveAsPNG"

}

// MARK: UI
extension Preferences {
    enum Key {
        case saveAsPNG, showAdvancedOptions, showIncompleteScanImages, previewWithAutoColorMode
        
        var localizedTitle: String {
            switch self {
            case .saveAsPNG:                return "PREFERENCES TITLE SAVE AS PNG".localized
            case .showAdvancedOptions:      return "PREFERENCES TITLE SHOW ADVANCED OPTIONS".localized
            case .showIncompleteScanImages: return "PREFERENCES TITLE SHOW INCOMPLETE SCAN".localized
            case .previewWithAutoColorMode: return "PREFERENCES TITLE PREVIEW DEFAULT COLOR MODE".localized
            }
        }
        
        var localizedDescription: String {
            switch self {
            case .saveAsPNG:                return "PREFERENCES MESSAGE SAVE AS PNG".localized
            case .showAdvancedOptions:      return "PREFERENCES MESSAGE SHOW ADVANCED OPTIONS".localized
            case .showIncompleteScanImages: return "PREFERENCES MESSAGE SHOW INCOMPLETE SCAN".localized
            case .previewWithAutoColorMode: return "PREFERENCES MESSAGE PREVIEW DEFAULT COLOR MODE".localized
            }
        }
    }
    
    var groupedKeys: [(String, [Key])] {
        return [
            ("PREFERENCES SECTION PREVIEW".localized, [.previewWithAutoColorMode, .showIncompleteScanImages, .saveAsPNG]),
            ("PREFERENCES SECTION SCAN".localized, [.showAdvancedOptions]),
        ]
    }
    
    // TODO: use subscript when Swift
    func value(for key: Key) -> Bool {
        switch key {
        case .saveAsPNG:                return self.saveAsPNG
        case .showAdvancedOptions:      return self.showAdvancedOptions
        case .showIncompleteScanImages: return self.showIncompleteScanImages
        case .previewWithAutoColorMode: return self.previewWithAutoColorMode
        }
    }
    
    func setValue(_ value: Bool, for key: Key) {
        switch key {
        case .saveAsPNG:                self.saveAsPNG = value
        case .showAdvancedOptions:      self.showAdvancedOptions = value
        case .showIncompleteScanImages: self.showIncompleteScanImages = value
        case .previewWithAutoColorMode: self.previewWithAutoColorMode = value
        }
    }
}
