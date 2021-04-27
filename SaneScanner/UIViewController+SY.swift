//
//  UIViewController+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 26/04/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

extension UIViewController {
    var useLargeLayout: Bool {
        return view.window?.traitCollection.horizontalSizeClass == .regular && view.window?.traitCollection.verticalSizeClass == .regular
    }
}
