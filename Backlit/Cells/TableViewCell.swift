//
//  TableViewCell.swift
//  Backlit
//
//  Created by syan on 09/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

class TableViewCell: UITableViewCell {
    override func awakeFromNib() {
        super.awakeFromNib()
        
        if UIDevice.isCatalyst {
            backgroundColor = .clear
        }
    }
}
