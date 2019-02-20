//
//  HostCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class HostCell: UITableViewCell {

    override func awakeFromNib() {
        super.awakeFromNib()
        
        accessoryType = .none
        if #available(iOS 10.0, *) {
            label.adjustsFontForContentSizeCategory = true
        }
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
            accessoryView = showAddIndicator ? UIImageView(image: UIImage(named: "button-add")) : nil
        }
    }
}
