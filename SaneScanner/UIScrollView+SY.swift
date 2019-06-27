//
//  UIScrollView+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit

extension UIScrollView {
    func addPullToResfresh(_ completion: @escaping ((UIScrollView) -> ())) {
        let control = DotsRefreshControl(frame: .zero)
        control.tintColor = UIColor.groupTableViewHeaderTitle.withAlphaComponent(0.8)
        control.attributedTitle = nil
        control.backgroundColor = .clear
        addPullToRefresh(control, action: completion)
    }
}
