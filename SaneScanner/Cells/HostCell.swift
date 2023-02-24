//
//  HostCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class HostCell: TableViewCell {

    override func awakeFromNib() {
        super.awakeFromNib()
        
        accessoryType = .none
        label.autoAdjustsFontSize = true
    }
    
    // MARK: Views
    @IBOutlet private var label: UILabel!
    
    // MARK: Properties
    var title: String? {
        get { return label.text }
        set { label.text = newValue }
    }
    
    var showAddIndicator: Bool = false {
        didSet {
            if showAddIndicator {
                let imageView = UIImageView(image: UIImage(named: "scanner"))
                imageView.tintColor = .normalText
                imageView.frame.size = .init(width: 20, height: 20)
                imageView.contentMode = .scaleAspectFit
                accessoryView = imageView
            }
            else {
                accessoryView = nil
            }
        }
    }
}
