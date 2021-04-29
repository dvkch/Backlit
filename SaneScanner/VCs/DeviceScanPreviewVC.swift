//
//  DeviceScanPreviewVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

class DeviceScanPreviewVC: UIAlertController {
    // MARK: Init
    required convenience init(shareTap: @escaping () -> (), cancelTap: @escaping () -> ()) {
        self.init(title: "DIALOG TITLE SCANNED IMAGE".localized, message: nil, preferredStyle: .alert)

        addAction(UIAlertAction(title: "ACTION SHARE".localized, style: .default, handler: { (_) in
            shareTap()
        }))
        cancelButton = UIAlertAction(title: "", style: .cancel, handler: { (_) in
            cancelTap()
        })
        addAction(cancelButton!)
        
        imageView = setupImageView(image: nil, height: 300, margins: UIEdgeInsets(top: 0, left: 0, bottom: 20, right: 0))
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        isScanning = true
    }
    
    // MARK: Private properties
    private var imageView: UIImageView?
    private var cancelButton: UIAlertAction?
    
    // MARK: Public properties
    var isScanning: Bool = false {
        didSet {
            if isScanning {
                cancelButton?.updateTitle("ACTION CANCEL".localized)
                actions.forEach { $0.isEnabled = $0.style == .cancel }
            } else {
                cancelButton?.updateTitle("ACTION CLOSE".localized)
                actions.forEach { $0.isEnabled = true }
            }
        }
    }
    
    var image: UIImage? {
        get { return imageView?.image }
        set { imageView?.image = newValue }
    }
}
