//
//  DotsRefreshControl.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 21/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

// TODO: Switch to localized strings swift gen
// TODO: cleanup code now that we vendored the animation FW

class DotsRefreshControl : UIRefreshControl {
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    private func setup() {
        isAccessibilityElement = true
        accessibilityLabel = "LOADING".localized
        accessibilityTraits = .staticText

        addSubview(dotsView)
        dotsView.snp.makeConstraints { (make) in
            make.center.equalToSuperview()
        }
        updateDotsView()
    }
    
    // MARK: Properties
    private let dotsView = ChasingDotsView()
    var spinnerSize = CGFloat(50) {
        didSet {
            updateDotsView()
        }
    }

    // MARK: Subclassing
    override var tintColor: UIColor! {
        didSet {
            updateDotsView()
        }
    }
    
    override var frame: CGRect {
        didSet {
            if frame.minY < 0 && !isRefreshing {
                dotsView.startAnimating()
                dotsView.alpha = abs(frame.minY) / (dotsView.frame.height * 1.5)
            }
        }
    }
    
    override func beginRefreshing() {
        super.beginRefreshing()
        dotsView.alpha = 1
        dotsView.startAnimating()
    }
    
    override func endRefreshing() {
        super.endRefreshing()
        dotsView.stopAnimating()
    }
    
    // MARK: Property changes
    private func updateDotsView() {
        dotsView.color = tintColor ?? .gray
        dotsView.spinnerSize = 50
        dotsView.hidesWhenStopped = false
    }
}

