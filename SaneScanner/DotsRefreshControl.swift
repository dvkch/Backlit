//
//  DotsRefreshControl.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 21/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SpinKit

class DotsRefreshControl : UIRefreshControl {
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        recreateDotsView()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        recreateDotsView()
    }
    
    // MARK: Properties
    private var dotsView: RTSpinKitView?
    var spinnerSize = CGFloat(50) {
        didSet {
            recreateDotsView()
        }
    }

    // MARK: Subclassing
    override var tintColor: UIColor! {
        didSet {
            recreateDotsView()
        }
    }
    
    override var frame: CGRect {
        didSet {
            if frame.minY < 0 && !isRefreshing {
                dotsView?.startAnimating()
                dotsView?.alpha = abs(frame.minY) / (dotsView!.frame.height * 1.5)
            }
        }
    }
    
    override func beginRefreshing() {
        super.beginRefreshing()
        if dotsView == nil {
            recreateDotsView()
        }
        dotsView?.alpha = 1
        dotsView?.startAnimating()
    }
    
    override func endRefreshing() {
        super.endRefreshing()
        dotsView?.stopAnimating()
    }
    
    // MARK: Property changes
    private func recreateDotsView() {
        subviews.forEach { $0.removeFromSuperview() }
        
        dotsView = RTSpinKitView(style: .styleChasingDots, color: tintColor ?? .gray, spinnerSize: 50)
        dotsView?.hidesWhenStopped = false
        addSubview(dotsView!)
        
        dotsView?.snp.makeConstraints { (make) in
            make.center.equalToSuperview()
        }
    }
}

