//
//  DeviceCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift

class DeviceCell: TableViewCell {
    
    override func awakeFromNib() {
        super.awakeFromNib()
        
        labelName.adjustsFontForContentSizeCategory = true
        labelName.textColor = .normalText
        labelDetails.adjustsFontForContentSizeCategory = true
        labelDetails.textColor = .altText
    }
    
    // MARK: Views
    @IBOutlet private var labelName: UILabel!
    @IBOutlet private var labelDetails: UILabel!

    // MARK: Properties
    var device: Device? {
        didSet {
            updateTexts()
        }
    }
    var index: Int = 0

    var isLoading: Bool = false {
        didSet {
            if isLoading {
                let spinner: UIActivityIndicatorView
                if #available(iOS 13.0, *) {
                    spinner = UIActivityIndicatorView(style: .medium)
                } else {
                    spinner = UIActivityIndicatorView(style: .white)
                }
                spinner.color = .normalText
                spinner.accessibilityLabel = "LOADING".localized
                accessoryView = spinner
                spinner.startAnimating()
                accessibilityIdentifier = "loading_device"
            }
            else {
                showDisclosureIndicator(index: index)
                accessibilityIdentifier = nil
            }
        }
    }
    
    private func updateTexts() {
        guard let device = device else { return }
        labelName.text = device.model
        labelDetails.text = [device.host.displayName, device.type]
            .compactMap { $0 }
            .joined(separator: " – ")
    }
}
