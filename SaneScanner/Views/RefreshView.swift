//
//  RefreshView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 13/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

struct RefreshView {
    init(tableView: UITableView, viewController: UIViewController, _ completion: @escaping () -> ()) {
        #if targetEnvironment(macCatalyst)
        self.viewController = viewController
        self.completionBlock = completion
        viewController.navigationItem.rightBarButtonItem = UIBarButtonItem(systemItem: .refresh, block: completion)
        #else
        self.tableView = tableView
        self.completionBlock = completion
        tableView.addPullToResfresh { _ in
            completion()
        }
        #endif
    }
    
    #if targetEnvironment(macCatalyst)
    weak var viewController: UIViewController?
    #else
    weak var tableView: UITableView?
    #endif
    var completionBlock: () -> ()

    
    func startLoading(discreet: Bool = false) {
        #if targetEnvironment(macCatalyst)
        let loader: UIActivityIndicatorView
        if #available(iOS 13.0, *) {
            loader = UIActivityIndicatorView(style: .medium)
        } else {
            loader = UIActivityIndicatorView(style: .white)
        }
        loader.color = .tint
        loader.startAnimating()
        loader.accessibilityLabel = "LOADING".localized
        viewController?.navigationItem.rightBarButtonItem = UIBarButtonItem(customView: loader)
        #else
        if !discreet {
            tableView?.showPullToRefresh()
        }
        #endif
    }
    
    func stopLoading() {
        #if targetEnvironment(macCatalyst)
        viewController?.navigationItem.rightBarButtonItem = UIBarButtonItem(systemItem: .refresh, block: completionBlock)
        #else
        tableView?.endPullToRefresh()
        #endif
    }
}
