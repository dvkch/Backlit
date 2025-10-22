//
//  RefreshView.swift
//  Backlit
//
//  Created by syan on 13/05/2021.
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
        tableView.addPullToRefresh { _ in
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
        let loader = ChasingDotsView()
        loader.spinnerSize = 20
        loader.color = .tint
        loader.startAnimating()
        loader.accessibilityLabel = L10n.loading
        viewController?.navigationItem.rightBarButtonItem = UIBarButtonItem(customView: loader)
        #else
        if !discreet {
            tableView?.showPullToRefresh()
        }
        #endif
    }
    
    func stopLoading() {
        #if targetEnvironment(macCatalyst)
        viewController?.navigationItem.rightBarButtonItem = .refresh(block: completionBlock)
        #else
        tableView?.endPullToRefresh()
        #endif
    }
}
