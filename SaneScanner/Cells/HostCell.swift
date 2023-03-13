//
//  HostCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift

class HostCell: TableViewCell {

    override func awakeFromNib() {
        super.awakeFromNib()
        
        accessoryType = .none
        label.autoAdjustsFontSize = true
    }
    
    // MARK: Views
    @IBOutlet private var nameLabel: UILabel!
    @IBOutlet private var hostLabel: UILabel!

    // MARK: Properties
    enum Kind {
        case saneHost(SaneHost)
        case bonjourHost(SaneHost)
        case add
    }

    var kind: Kind = .add {
        didSet {
            updateContent()
        }
    }
    
    private func updateContent() {
        var indicator: UIImage? = nil
        
        switch kind {
        case .saneHost(let host):
            nameLabel.text = host.displayName
            hostLabel.text = host.hostname
            
        case .bonjourHost(let host):
            nameLabel.text = host.displayName
            hostLabel.text = host.hostname
            indicator = UIImage(named: "network")

        case .add:
            nameLabel.text = "DEVICES ROW ADD HOST".localized
            hostLabel.text = nil
            indicator = UIImage(named: "scanner")
        }
        
        if let indicator {
            let imageView = UIImageView(image: indicator)
            imageView.tintColor = nameLabel.textColor
            imageView.frame.size = .init(width: 20, height: 20)
            imageView.contentMode = .scaleAspectFit
            accessoryView = imageView
        }
        else {
            accessoryView = nil
        }
    }
}
