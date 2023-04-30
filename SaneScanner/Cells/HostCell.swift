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
        nameLabel.adjustsFontForContentSizeCategory = true
        hostLabel.adjustsFontForContentSizeCategory = true
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
            indicator = .icon(.network)

        case .add:
            nameLabel.text = "DEVICES ROW ADD HOST".localized
            hostLabel.text = nil
            indicator = .icon(.scanner)
        }
        
        if let indicator {
            let imageView = UIImageView(image: indicator)
            imageView.tintColor = nameLabel.textColor
            imageView.contentMode = .scaleAspectFit
            let size = UIFontMetrics.default.scaledValue(for: 20)
            imageView.frame.size = .init(width: size, height: size)
            accessoryView = imageView
        }
        else {
            accessoryView = nil
        }
    }
}
