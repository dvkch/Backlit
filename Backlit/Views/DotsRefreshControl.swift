//
//  DotsRefreshControl.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 21/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

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
        accessibilityLabel =  L10n.loading
        accessibilityTraits = .staticText

        dotsView.color = tintColor ?? .gray
        dotsView.hidesWhenStopped = false
        dotsView.spinnerSize = 50
        addSubview(dotsView)
        dotsView.snp.makeConstraints { (make) in
            make.center.equalToSuperview()
            make.size.equalTo(dotsView.spinnerSize)
        }
    }
    
    // MARK: Properties
    private let dotsView = ChasingDotsView()

    // MARK: Subclassing
    override var tintColor: UIColor! {
        didSet {
            dotsView.color = tintColor ?? .gray
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
}

