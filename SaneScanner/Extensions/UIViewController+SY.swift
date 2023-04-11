//
//  UIViewController+SY.swift
//  SaneScanner
//
//  Created by syan on 11/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

extension UIViewController {
    var isVisible: Bool {
        return viewIfLoaded?.window != nil
    }
}
