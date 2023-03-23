//
//  AcknowledgementCell.swift
//  SaneScanner
//
//  Created by syan on 19/03/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

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
    }
    
    // MARK: Views
    @IBOutlet private var libraryNameLabel: UILabel!
    @IBOutlet private var licenseNameLabel: UILabel!
    @IBOutlet private var licenseTextLabel: UILabel!
    @IBOutlet private var disclosureImageView: UIImageView!

    // MARK: Properties
    var acknowledgement: Acknowledgement? {
        didSet {
            updateTexts()
        }
    }
    var showDescription: Bool = false {
        didSet {
            licenseTextLabel.sy_isHidden = !showDescription
            disclosureImageView.image = UIImage(named: showDescription ? "chevron.up" : "chevron.down")
        }
    }
    
    private func updateTexts() {
        guard let acknowledgement else { return }
        libraryNameLabel.text = acknowledgement.title
        licenseNameLabel.text = acknowledgement.licenseName
        licenseTextLabel.text = acknowledgement.licenseText.trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
