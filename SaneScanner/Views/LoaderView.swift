//
//  LoaderView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 13/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

enum LoaderView {
    case pullToRefresh(UITableView, () -> ())
    case barButtonItem(UIViewController, () -> ())
    
    init(tableView: UITableView, viewController: UIViewController, _ completion: @escaping () -> ()) {
        #if targetEnvironment(macCatalyst)
        self = .barButtonItem(viewController, completion)
        viewController.navigationItem.rightBarButtonItem = UIBarButtonItem(systemItem: .refresh, block: completion)
        #else
        self = .pullToRefresh(tableView, completion)
        tableView.addPullToResfresh { _ in
            completion()
        }
        #endif
    }
    
    func startLoading(discreet: Bool = false) {
        switch self {
        case .barButtonItem(let viewController, _):
            let loader: UIActivityIndicatorView
            if #available(iOS 13.0, *) {
                loader = UIActivityIndicatorView(style: .medium)
            } else {
                loader = UIActivityIndicatorView(style: .white)
            }
            loader.color = .tint
            loader.startAnimating()
            loader.accessibilityLabel = "LOADING".localized
            viewController.navigationItem.rightBarButtonItem = UIBarButtonItem(customView: loader)
            
        case .pullToRefresh(let tableView, _):
            if !discreet {
                tableView.showPullToRefresh()
            }
        }
    }
    
    func stopLoading() {
        switch self {
        case .barButtonItem(let viewController, let block):
            viewController.navigationItem.rightBarButtonItem = UIBarButtonItem(systemItem: .refresh, block: block)

        case .pullToRefresh(let tableView, _):
            tableView.endPullToRefresh()
        }
    }
}
