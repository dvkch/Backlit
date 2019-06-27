//
//  UIScrollView+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import ObjectiveC

private var UIScrollView_refreshControlAction: UInt8 = 0

// MARK: Pull to refresh
@available(tvOS, unavailable)
public extension UIScrollView {
    
    typealias SYKitUIScrollViewRefreshAction = (UIScrollView) -> ()
    
    @objc(sy_refreshControlAction)
    var refreshControlAction: SYKitUIScrollViewRefreshAction? {
        get {
            return objc_getAssociatedObject(self, &UIScrollView_refreshControlAction) as? SYKitUIScrollViewRefreshAction
        }
        set {
            objc_setAssociatedObject(self, &UIScrollView_refreshControlAction, newValue, .OBJC_ASSOCIATION_COPY_NONATOMIC)
        }
    }
    
    var sy_refreshControl: UIRefreshControl? {
        get {
            if #available(iOS 10.0, *) {
                return refreshControl
            } else {
                return subviews.compactMap { $0 as? UIRefreshControl }.first
            }
        }
        set {
            if #available(iOS 10.0, *) {
                self.refreshControl = newValue
            } else {
                sy_refreshControl?.removeFromSuperview()
                if let newValue = newValue {
                    self.addSubview(newValue)
                }
            }
        }
    }
    
    @objc(sy_addPullToRefreshWithColor:action:)
    func addPullToRefresh(color: UIColor?, action: @escaping SYKitUIScrollViewRefreshAction) {
        let control = UIRefreshControl()
        control.tintColor = color
        control.attributedTitle = nil
        control.backgroundColor = .clear
        addPullToRefresh(control, action: action)
    }
    
    @objc(sy_addPullToRefresh:action:)
    func addPullToRefresh(_ control: UIRefreshControl, action: @escaping SYKitUIScrollViewRefreshAction) {
        self.refreshControlAction = action
        
        sy_refreshControl?.removeFromSuperview()
        sy_refreshControl = nil
        
        control.addTarget(self, action: #selector(sy_refreshControlValueChanged), for: .valueChanged)
        
        sy_refreshControl = control
    }
    
    @objc private func sy_refreshControlValueChanged() {
        guard let action = refreshControlAction else { return }
        action(self)
    }
    
    @objc(sy_showPullToRefresh)
    func showPullToRefresh() {
        guard let sy_refreshControl = sy_refreshControl else { return }
        
        sy_refreshControl.beginRefreshing()
        // 60 is the average height for the refreshControl. we can't use UIRefreshControl.frame.size.height because
        // when it is closed its height is 0.5px...
        setContentOffset(CGPoint(x: 0, y: -60), animated: true)
    }
    
    @objc(sy_endPullToRefresh)
    func endPullToRefresh() {
        sy_refreshControl?.endRefreshing()
    }
}


