//
//  SVProgressHUD+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

#if !targetEnvironment(macCatalyst)
import SVProgressHUD

extension SVProgressHUD {
    
    static func applyStyle(initial: Bool = true) {
        setDefaultMaskType(.black)
        setForegroundColor(.normalText)
        setBackgroundColor(.backgroundAlt)
        setFont(UIFont.preferredFont(forTextStyle: .body))
        NotificationCenter.default.addObserver(self, selector: #selector(traitsChangedNotification), name: UIContentSizeCategory.didChangeNotification, object: nil)
    }
    
    static func showSuccess(status: String?, duration: TimeInterval) {
        showSuccess(withStatus: status)
        dismiss(withDelay: duration)
    }
    
    @objc static func traitsChangedNotification() {
        applyStyle(initial: false)
    }
}
#endif
