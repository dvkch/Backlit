//
//  DeviceCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift

class DeviceCell: UITableViewCell {
    
    override func awakeFromNib() {
        super.awakeFromNib()
        
        if #available(iOS 10.0, *) {
            labelName.adjustsFontForContentSizeCategory = true
            labelDetails.adjustsFontForContentSizeCategory = true
        }
    }
    
    // MARK: Views
    @IBOutlet private var labelName: UILabel!
    @IBOutlet private var labelDetails: UILabel!

    // MARK: Properties
    @objc var device: SYSaneDevice? {
        didSet {
            updateTexts()
        }
    }
    
    private func updateTexts() {
        guard let device = device else { return }
        labelName.text = device.model
        labelDetails.text = [device.host(), device.type]
            .compactMap { $0 }
            .joined(separator: " – ")
    }
}
