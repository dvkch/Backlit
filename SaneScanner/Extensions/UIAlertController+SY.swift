//
//  UIAlertController+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 09/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit
import SaneSwift

extension UIAlertController {
    static func show(for error: Error, title: String? = nil, in viewController: UIViewController) {
        if case .cancelled = error as? SaneError {
            return
        }

        let alert = self.init(title: title, message: error.localizedDescription, preferredStyle: .alert)
        alert.addAction(title: "ACTION CLOSE".localized, style: .cancel, handler: nil)
        viewController.present(alert, animated: true, completion: nil)
    }
    
    static func show(title: String? = nil, message: String? = nil, in viewController: UIViewController) {
        let alert = self.init(title: title, message: message, preferredStyle: .alert)
        alert.addAction(title: "ACTION CLOSE".localized, style: .cancel, handler: nil)
        viewController.present(alert, animated: true, completion: nil)
    }
}

class HUDAlertController: UIAlertController {
    // MARK: Convenience
    static func show(in viewController: UIViewController, animated: Bool = true) -> Self {
        let alert = self.init(title: nil, message: "", preferredStyle: .alert)
        viewController.present(alert, animated: false, completion: nil)
        return alert
    }
    
    static func dismiss(_ hud: HUDAlertController?, completion: @escaping () -> ()) {
        if let hud = hud {
            hud.dismiss(animated: true, completion: completion)
        }
        else {
            completion()
        }
    }
    
    // MARK: Init
    override func viewDidLoad() {
        super.viewDidLoad()
        loader.startAnimating()
        view.addSubview(loader)
        loader.snp.makeConstraints { make in
            make.center.equalToSuperview()
        }
    }

    // MARK: Properties
    let preferredSize = CGSize(width: 100, height: 100)
    
    private var contentView: UIView {
        return view.subviews.first!
    }
    
    // MARK: Views
    private lazy var loader: UIActivityIndicatorView = {
        let loader: UIActivityIndicatorView
        if #available(iOS 13.0, *) {
            loader = UIActivityIndicatorView(style: .large)
        } else {
            loader = UIActivityIndicatorView(style: .whiteLarge)
        }
        loader.color = .normalText
        return loader
    }()

    // MARK: Layout
    override func updateViewConstraints() {
        let contentViewWidth = contentView.constraints.constantAttribute(.width).first ?? contentView.widthAnchor.constraint(equalToConstant: 0)
        contentViewWidth.priority = .defaultLow
        contentViewWidth.constant = preferredSize.width
        contentViewWidth.isActive = true
        
        let viewWidth = view.constraints.constantAttribute(.width).first ?? view.widthAnchor.constraint(equalToConstant: 0)
        viewWidth.constant = preferredSize.width
        viewWidth.isActive = true
        
        let viewHeight = view.constraints.constantAttribute(.height).first ?? view.heightAnchor.constraint(equalToConstant: 0)
        viewHeight.constant = preferredSize.width
        viewHeight.isActive = true

        super.updateViewConstraints()
    }
}

private extension Array where Element == NSLayoutConstraint {
    func constantAttribute(_ attribute: NSLayoutConstraint.Attribute, relation: NSLayoutConstraint.Relation = .equal) -> [Element] {
        return filter { $0.firstAttribute == attribute && $0.relation == relation && $0.secondItem == nil && $0.secondAnchor == nil }
    }
}
