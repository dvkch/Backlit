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
        
        labelName.autoAdjustsFontSize = true
        labelName.textColor = .normalText
        labelDetails.autoAdjustsFontSize = true
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
    
    private func updateTexts() {
        guard let device = device else { return }
        labelName.text = device.model
        labelDetails.text = [device.host, device.type]
            .compactMap { $0 }
            .joined(separator: " – ")
    }
}
