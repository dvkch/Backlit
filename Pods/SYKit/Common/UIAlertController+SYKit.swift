//
//  UIAlertController+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 04/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

@available(iOS 8.0, tvOS 9.0, *)
extension UIAlertController {
    
    @discardableResult
    @objc(sy_addActionWithTitle:style:handler:)
    public func addAction(title: String?, style: UIAlertAction.Style, handler: ((UIAlertAction) -> ())?) -> UIAlertAction {
        let action = UIAlertAction(title: title, style: style, handler: handler)
        self.addAction(action)
        return action
    }

    @objc(sy_setContentViewController:height:)
    public func setContentViewController(_ contentVC: UIViewController?, height: CGFloat = -1) {
        setValue(contentVC, forKey: "contentViewController")
        
        if contentVC != nil, height > 0 {
            contentVC?.preferredContentSize.height = height
            preferredContentSize.height = height
        }
    }

    @discardableResult
    @objc(sy_setupImageViewWithImage:height:margins:)
    public func setupImageView(image: UIImage?, height: CGFloat, margins: UIEdgeInsets = .zero) -> UIImageView {
        let vc = SYImageViewController()
        vc.imageView.image = image
        vc.imageViewMargins = margins
        setContentViewController(vc, height: height)
        return vc.imageView
    }
}

@available(iOS 8.0, tvOS 9.0, *)
extension UIAlertAction {
    @objc(sy_updateTitle:)
    public func updateTitle(_ title: String) {
        setValue(title, forKey: "title")
    }
}

fileprivate class SYImageViewController : UIViewController {
    public override func viewDidLoad() {
        super.viewDidLoad()
        
        imageView.contentMode = .scaleAspectFit
        imageView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(imageView)
        
        topConstraint = NSLayoutConstraint(item: imageView, attribute: .top, relatedBy: .equal, toItem: view, attribute: .top, multiplier: 1, constant: 0)
        leftConstraint = NSLayoutConstraint(item: imageView, attribute: .left, relatedBy: .equal, toItem: view, attribute: .left, multiplier: 1, constant: 0)
        rightConstraint = NSLayoutConstraint(item: imageView, attribute: .right, relatedBy: .equal, toItem: view, attribute: .right, multiplier: 1, constant: 0)
        bottomConstraint = NSLayoutConstraint(item: imageView, attribute: .bottom, relatedBy: .equal, toItem: view, attribute: .bottom, multiplier: 1, constant: 0)
        
        NSLayoutConstraint.activate([topConstraint!, leftConstraint!, rightConstraint!, bottomConstraint!])
        updateImageViewMargins()
    }
    
    let imageView = UIImageView()
    var imageViewMargins: UIEdgeInsets = .zero {
        didSet {
            updateImageViewMargins()
        }
    }
    
    private var topConstraint: NSLayoutConstraint?
    private var leftConstraint: NSLayoutConstraint?
    private var rightConstraint: NSLayoutConstraint?
    private var bottomConstraint: NSLayoutConstraint?
    
    private func updateImageViewMargins() {
        topConstraint?.constant = imageViewMargins.top
        leftConstraint?.constant = imageViewMargins.left
        rightConstraint?.constant = -imageViewMargins.right
        bottomConstraint?.constant = -imageViewMargins.bottom
    }
}
