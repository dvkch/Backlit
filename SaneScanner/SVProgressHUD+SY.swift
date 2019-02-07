//
//  SVProgressHUD+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import SVProgressHUD

extension SVProgressHUD {

    static func showSuccess(status: String?, duration: TimeInterval) {
        showSuccess(withStatus: status)
        dismiss(withDelay: duration)
    }

}
