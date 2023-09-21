//
//  UIActivityViewController+SY.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

#if !targetEnvironment(macCatalyst)
import UIKit

extension UIActivityViewController {
    
    enum Sender {
        case barButtonItem(UIBarButtonItem)
        case view(UIView?)
    }

    static func showForURLs(_ urls: [URL], from sender: Sender, presentingVC: UIViewController, completion: (() -> ())? = nil) {
        var pdfActivities = [PDFActivity(interleaved: false)]
        if urls.count > 2 {
            pdfActivities.append(PDFActivity(interleaved: true))
        }

        let vc = UIActivityViewController(activityItems: urls, applicationActivities: pdfActivities)
        
        vc.completionWithItemsHandler = { activityType, completed, returnedItems, error in
            if let error = error {
                UIAlertController.show(for: error, in: presentingVC)
            }
            else if completed {
                completion?()
            }
        }
        
        switch sender {
        case .barButtonItem(let barButtonItem):
            vc.popoverPresentationController?.barButtonItem = barButtonItem
            
        case .view(let view):
            vc.popoverPresentationController?.permittedArrowDirections = .any
            if let view {
                vc.popoverPresentationController?.sourceView = view
                vc.popoverPresentationController?.sourceRect = view.bounds
            }
            else {
                vc.popoverPresentationController?.sourceView = presentingVC.view
                vc.popoverPresentationController?.sourceRect = CGRect(
                    x: presentingVC.view.bounds.width / 2,
                    y: presentingVC.view.bounds.height,
                    width: 1, height: 1
                )
            }
        }

        for pdfActivity in pdfActivities {
            pdfActivity.presentingViewController = presentingVC
            pdfActivity.configure(using: vc)
        }
        
        presentingVC.present(vc, animated: true, completion: nil)
    }
}
#endif
