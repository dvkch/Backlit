//
//  UIScrollView+SY.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit

extension UIScrollView {
    func addPullToRefresh(_ completion: @escaping ((UIScrollView) -> ())) {
        let control = DotsRefreshControl(frame: .zero)
        control.tintColor = UIColor.pullToRefresh.withAlphaComponent(0.8)
        control.attributedTitle = nil
        control.backgroundColor = .clear
        addPullToRefresh(control, action: completion)
    }
}
