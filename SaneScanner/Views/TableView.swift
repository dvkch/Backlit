//
//  TableView.swift
//  SaneScanner
//
//  Created by syan on 14/03/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

class TableView: UITableView {
    var shouldDelayTouch: ((UIView) -> Bool?)? {
        didSet {
            if shouldDelayTouch != nil {
                delaysContentTouches = false
            }
        }
    }

    override func touchesShouldCancel(in view: UIView) -> Bool {
        return shouldDelayTouch?(view) ?? super.touchesShouldCancel(in: view)
    }

    override func gestureRecognizerShouldBegin(_ gestureRecognizer: UIGestureRecognizer) -> Bool {
        if gestureRecognizer == panGestureRecognizer,
            let hitView = hitTest(gestureRecognizer.location(in: self), with: nil),
            let shouldDelay = shouldDelayTouch?(hitView)
        {
            return shouldDelay
        }
        
        return super.gestureRecognizerShouldBegin(gestureRecognizer)
    }
}
