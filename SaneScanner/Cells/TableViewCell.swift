//
//  TableViewCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 09/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

class TableViewCell: UITableViewCell {
    override func awakeFromNib() {
        super.awakeFromNib()
        
        #if targetEnvironment(macCatalyst)
        backgroundColor = .clear
        #endif
    }
}
