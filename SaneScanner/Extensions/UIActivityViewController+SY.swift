//
//  UIActivityViewController+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

#if !targetEnvironment(macCatalyst)
import SVProgressHUD
#endif


extension UIActivityViewController {

    static func showForURLs(_ urls: [URL], from barButtonItem: UIBarButtonItem, presentingVC: UIViewController, completion: (() -> ())?) {
        let pdfActivity = PDFActivity()
        
        let vc = UIActivityViewController(activityItems: urls, applicationActivities: [pdfActivity])
        
        vc.completionWithItemsHandler = { activityType, completed, returnedItems, error in
            if let error = error {
                SVProgressHUD.showError(withStatus: error.localizedDescription)
            }
            else if completed {
                completion?()
            }
        }
        
        vc.popoverPresentationController?.barButtonItem = barButtonItem
        pdfActivity.presentingViewController = presentingVC
        pdfActivity.configure(using: vc)
        
        presentingVC.present(vc, animated: true, completion: nil)
    }


    static func showForURLs(_ urls: [URL], fromBottomIn presentingVC: UIViewController, completion: (() -> ())?) {
        let pdfActivity = PDFActivity()
        
        let vc = UIActivityViewController(activityItems: urls, applicationActivities: [pdfActivity])
        
        vc.completionWithItemsHandler = { activityType, completed, returnedItems, error in
            if let error = error {
                SVProgressHUD.showError(withStatus: error.localizedDescription)
            }
            else if completed {
                completion?()
            }
        }
        
        vc.popoverPresentationController?.permittedArrowDirections = .up
        vc.popoverPresentationController?.sourceView = presentingVC.view
        vc.popoverPresentationController?.sourceRect = CGRect(x: presentingVC.view.bounds.width / 2, y: presentingVC.view.bounds.height, width: 1, height: 1)
        pdfActivity.presentingViewController = presentingVC
        pdfActivity.configure(using: vc)
        
        presentingVC.present(vc, animated: true, completion: nil)
    }
}
