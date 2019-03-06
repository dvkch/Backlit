//
//  SVProgressHUD+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

#if !MARZIPAN
import SVProgressHUD
#else
class SVProgressHUD {
    enum MaskType { case black }
    static func setDefaultMaskType(_ mask: MaskType) {}

    static func show() { }
    static func show(withStatus status: String?) { }
    static func showSuccess(withStatus status: String?) { }
    static func showProgress(_ progress: Float) { }
    static func showError(withStatus: String?) { }

    static func dismiss() { }
    static func dismiss(withDelay: TimeInterval) { }
}
#endif

extension SVProgressHUD {
    
    static func showSuccess(status: String?, duration: TimeInterval) {
        showSuccess(withStatus: status)
        dismiss(withDelay: duration)
    }
    
}


