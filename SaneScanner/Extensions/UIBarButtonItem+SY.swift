//
//  UIBarButtonItem+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 13/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import ObjectiveC

// MARK: Common items

// TODO: test all with VoiceOver
// TODO: test all with large content viewer
extension UIBarButtonItem {
    static var emptyBack: UIBarButtonItem {
        return UIBarButtonItem(title: "", style: .plain, target: nil, action: nil)
    }
    
    static func edit(target: Any, action: Selector) -> UIBarButtonItem {
        return UIBarButtonItem(barButtonSystemItem: .edit, target: target, action: action)
    }

    static func done(target: Any, action: Selector) -> UIBarButtonItem {
        let button = UIBarButtonItem(barButtonSystemItem: .done, target: target, action: action)
        button.style = .done
        return button
    }

    static func close(target: Any, action: Selector) -> UIBarButtonItem {
        if #available(iOS 13.0, *) {
            return UIBarButtonItem(barButtonSystemItem: .close, target: target, action: action)
        } else {
            return UIBarButtonItem(barButtonSystemItem: .stop, target: target, action: action)
        }
    }

    static func settings(target: Any, action: Selector) -> UIBarButtonItem {
        let button = UIBarButtonItem(
            image: UIImage(named: "settings"), style: .plain,
            target: target, action: action
        )
        button.title = "PREFERENCES TITLE".localized
        return button
    }
    
    static func openFolder(target: Any, action: Selector) -> UIBarButtonItem {
        if #available(iOS 13.0, *) {
            return UIBarButtonItem(image: UIImage(systemName: "folder"), style: .plain, target: target, action: action)
        } else {
            return UIBarButtonItem(image: UIImage(named: "folder"), style: .plain, target: target, action: action)
        }
    }
    
    static var flexibleSpace: UIBarButtonItem {
        return UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil)
    }
    
    static func share(target: Any, action: Selector) -> UIBarButtonItem {
        return UIBarButtonItem(barButtonSystemItem: .action, target: target, action: action)
    }
    
    static func delete(target: Any, action: Selector) -> UIBarButtonItem {
        return UIBarButtonItem(barButtonSystemItem: .trash, target: target, action: action)
    }
}

// MARK: Convenience methods
private var UIBarButtonItemTapBlock: UInt8?
extension UIBarButtonItem {
    convenience init(systemItem: UIBarButtonItem.SystemItem, block: @escaping (() -> ())) {
        self.init(barButtonSystemItem: systemItem, target: nil, action: nil)
        self.tapBlock = block
    }
    
    var tapBlock: (() -> ())? {
        get { objc_getAssociatedObject(self, &UIBarButtonItemTapBlock) as? (() -> ()) }
        set {
            objc_setAssociatedObject(self, &UIBarButtonItemTapBlock, newValue, .OBJC_ASSOCIATION_COPY)
            self.target = self
            self.action = #selector(tapBlockCallback)
        }
    }
    
    @objc private func tapBlockCallback() {
        tapBlock?()
    }
}
