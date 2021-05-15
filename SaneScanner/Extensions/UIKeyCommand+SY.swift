//
//  UIKeyCommand+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 14/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

extension UIKeyCommand {
    convenience init(title: String, action: Selector, input: String, modifierFlags: UIKeyModifierFlags) {
        self.init(input: input, modifierFlags: modifierFlags, action: action)
        if #available(iOS 13.0, *) {
            self.title = title
        }
    }

    static var preview: UIKeyCommand {
        return .init(title: "MENU PREVIEW".localized, action: #selector(DeviceVC.preview), input: "P", modifierFlags: .command)
    }

    static var scan: UIKeyCommand {
        return .init(title: "MENU SCAN".localized, action: #selector(DeviceVC.scan), input: "S", modifierFlags: .command)
    }

    static var abort: UIKeyCommand {
        return .init(title: "MENU ABORT".localized, action: #selector(DeviceVC.cancelOperation), input: UIKeyCommand.inputEscape, modifierFlags: .command)
    }

    static var openGallery: UIKeyCommand {
        return .init(title: "MENU OPEN GALLERY".localized, action: #selector(SplitVC.openGallery), input: "O", modifierFlags: .command)
    }

    static var addHost: UIKeyCommand {
        return .init(title: "MENU ADD HOST".localized, action: #selector(DevicesVC.addHostButtonTap), input: "N", modifierFlags: .command)
    }

    static var refresh: UIKeyCommand {
        return .init(title: "MENU REFRESH".localized, action: #selector(DevicesVC.refresh), input: "R", modifierFlags: .command)
    }

    static var settings: UIKeyCommand {
        return .init(title: "MENU PREFERENCES".localized, action: #selector(DevicesVC.settingsButtonTap), input: ",", modifierFlags: .command)
    }
    
    static var close: UIKeyCommand {
        return .init(title: "ACTION CLOSE".localized, action: #selector(PreferencesVC.closeButtonTap), input: UIKeyCommand.inputEscape, modifierFlags: .init())
    }
}

@available(iOS 13.0, *)
extension Array where Element: UIMenuElement {
    func asMenu(identifier: UIMenu.Identifier? = nil) -> UIMenu {
        return UIMenu(title: "", image: nil, identifier: identifier, options: .displayInline, children: self)
    }
}
