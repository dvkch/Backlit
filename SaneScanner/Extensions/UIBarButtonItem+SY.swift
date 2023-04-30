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
extension UIBarButtonItem {
    @available(iOS, obsoleted: 14.0, message: "Use navigationItem.backButtonDisplayMode = .minimal")
    static func back(title: String = "") -> UIBarButtonItem {
        return UIBarButtonItem(title: title, style: .plain, target: nil, action: nil)
    }
    
    static func edit(target: Any, action: Selector) -> UIBarButtonItem {
        return UIBarButtonItem(barButtonSystemItem: .edit, target: target, action: action)
    }
    
    static func done(target: Any, action: Selector) -> UIBarButtonItem {
        return UIBarButtonItem(barButtonSystemItem: .done, target: target, action: action)
    }
    
    static func refresh(block: @escaping () -> ()) -> UIBarButtonItem {
        let button = UIBarButtonItem(image: .icon(.refresh), style: .plain, target: nil, action: nil)
        button.accessibilityLabel = "ACTION REFRESH".localized
        button.tapBlock = block
        return button
    }
    
    static func close(target: Any, action: Selector) -> UIBarButtonItem {
        let button = UIButton(type: .system)
        button.accessibilityLabel = "ACTION CLOSE".localized
        button.backgroundColor = .normalText.withAlphaComponent(0.1)
        button.setImage(.icon(.close, variant: nil), for: .normal)
        button.tintColor = .altText
        button.adjustsImageSizeForAccessibilityContentSizeCategory = true
        button.widthAnchor.constraint(equalTo: button.heightAnchor, multiplier: 1).isActive = true
        button.widthAnchor.constraint(equalToConstant: 30).isActive = true
        button.layer.cornerRadius = 15
        button.addTarget(target, action: action, for: .primaryActionTriggered)
        return UIBarButtonItem(customView: button)
    }
    
    static func settings(target: Any, action: Selector) -> UIBarButtonItem {
        let button = UIBarButtonItem(image: .icon(.settings), style: .plain, target: target, action: action)
        button.title = "PREFERENCES TITLE".localized
        return button
    }
    
    static func openFolder(target: Any, action: Selector) -> UIBarButtonItem {
        let button = UIBarButtonItem(image: .icon(.open), style: .plain, target: target, action: action)
        button.title = "MENU OPEN GALLERY".localized
        return button
    }
    
    static func share(target: Any, action: Selector) -> UIBarButtonItem {
        let button = UIBarButtonItem(image: .icon(.share), style: .plain, target: target, action: action)
        button.title = "ACTION SHARE".localized
        return button
    }
    
    static func delete(target: Any, action: Selector) -> UIBarButtonItem {
        let button = UIBarButtonItem(image: .icon(.delete), style: .plain, target: target, action: action)
        button.title = "ACTION DELETE".localized
        return button
    }
    
    static var flexibleSpace: UIBarButtonItem {
        return UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil)
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
