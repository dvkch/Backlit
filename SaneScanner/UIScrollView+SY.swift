//
//  UIScrollView+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import ObjectiveC

private var UIScrollViewRefreshControlKey = UInt8(0)
private var UIScrollViewRefreshControlActionKey = UInt8(0)

extension UIScrollView {

    // MARK: Refresh control property
    @objc private var sy_refreshControl: UIRefreshControl? {
        get { return objc_getAssociatedObject(self, &UIScrollViewRefreshControlKey) as? UIRefreshControl }
        set { objc_setAssociatedObject(self, &UIScrollViewRefreshControlKey, newValue, .OBJC_ASSOCIATION_RETAIN_NONATOMIC) }
    }
    
    @objc private var sy_refreshControlAction: ((UIScrollView) -> ())? {
        get { return objc_getAssociatedObject(self, &UIScrollViewRefreshControlActionKey) as? ((UIScrollView) -> ()) }
        set { objc_setAssociatedObject(self, &UIScrollViewRefreshControlActionKey, newValue, .OBJC_ASSOCIATION_COPY) }
    }
    
    @objc func sy_addPullToResfresh(_ completion: ((UIScrollView) -> ())?) {
        self.sy_refreshControlAction = completion
        
        self.sy_refreshControl?.removeFromSuperview()
        self.sy_refreshControl = nil
    
        // TODO: [[RTSpinKitView alloc] initWithStyle:RTSpinKitViewStyleChasingDots color:color spinnerSize:kSpinnerSize]

        let control = UIRefreshControl(frame: .zero)
        control.tintColor = UIColor.groupTableViewHeaderTitle.withAlphaComponent(0.8)
        control.attributedTitle = nil
        control.backgroundColor = .clear
        control.addTarget(self, action: #selector(self.sy_refreshControlValueChanged), for: .valueChanged)
    
        if #available(iOS 10, *) {
            self.refreshControl = control
        }
        else {
            addSubview(control)
        }

        self.sy_refreshControl = control
    }
    
    @objc private func sy_refreshControlValueChanged() {
        if let block = sy_refreshControlAction {
            block(self)
        }
    }

    @objc func sy_showPullToRefresh(runBlock: Bool) {
        guard let control = sy_refreshControl, !control.isRefreshing else { return }
        
        control.beginRefreshing()
        
        // 60 is the average height for the refreshControl. we can't use UIRefreshControl.frame.size.height because
        // when it is closed its height is 0.5px...
        setContentOffset(CGPoint(x: 0, y: -60), animated: true)
    
        if runBlock, let block = sy_refreshControlAction {
            block(self)
        }
    }
    
    @objc func sy_endPullToRefresh() {
        sy_refreshControl?.endRefreshing()
    }
}
