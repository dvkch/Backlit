//
//  AcknowledgementCell.swift
//  Backlit
//
//  Created by syan on 19/03/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import LicenseList

class AcknowledgementCell: UITableViewCell {
    
    override func awakeFromNib() {
        super.awakeFromNib()
        backgroundColor = .cellBackground
        libraryNameLabel.adjustsFontForContentSizeCategory = true
        libraryNameLabel.textColor = .normalText
        licenseNameLabel.adjustsFontForContentSizeCategory = true
        licenseNameLabel.textColor = .altText
        licenseTextLabel.adjustsFontForContentSizeCategory = true
        licenseTextLabel.textColor = .altText
        disclosureImageView.tintColor = .altText
        disclosureImageView.adjustsImageSizeForAccessibilityContentSizeCategory = true
        disclosureImageView.transform = .init(scaleX: 0.7, y: 0.7)
    }
    
    // MARK: Views
    @IBOutlet private var libraryNameLabel: UILabel!
    @IBOutlet private var licenseNameLabel: UILabel!
    @IBOutlet private var licenseTextLabel: UILabel!
    @IBOutlet private var disclosureImageView: UIImageView!

    // MARK: Properties
    var index: Int = 0 {
        didSet {
            updateDisclosureIndicator()
        }
    }
    var acknowledgement: LicenseList.Library? {
        didSet {
            updateTexts()
        }
    }
    func update(using acknowledgement: LicenseList.Library, index: Int) {
        self.acknowledgement = acknowledgement
        self.index = index
    }

    var showDescription: Bool = false {
        didSet {
            licenseTextLabel.sy_isHidden = !showDescription
            updateDisclosureIndicator()
        }
    }
    
    private func updateTexts() {
        guard let acknowledgement else { return }
        libraryNameLabel.text = acknowledgement.name
        licenseNameLabel.text = acknowledgement.url?.absoluteString
        licenseTextLabel.text = acknowledgement.licenseBody.trimmingCharacters(in: .whitespacesAndNewlines)
    }
    
    private func updateDisclosureIndicator() {
        disclosureImageView.image = .icon(showDescription ? .up : .down, variant: index)
    }
}
