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
    struct Host {
        enum Kind {
            case saneConfig, bonjour, add
        }
        let kind: Kind
        let value: String
    }

    var host: Host? {
        didSet {
            updateContent()
        }
    }
    
    private func updateContent() {
        guard let host else { return }

        var indicator: UIImage? = nil
        
        switch host.kind {
        case .saneConfig:
            label.text = host.value
            label.textColor = .normalText

        case .bonjour:
            label.text = host.value
            label.textColor = .altText
            indicator = UIImage(named: "network")
            
        case .add:
            label.text = "DEVICES ROW ADD HOST".localized
            label.textColor = .normalText
            indicator = UIImage(named: "scanner")
        }
        
        if let indicator {
            let imageView = UIImageView(image: indicator)
            imageView.tintColor = label.textColor
            imageView.frame.size = .init(width: 20, height: 20)
            imageView.contentMode = .scaleAspectFit
            accessoryView = imageView
        }
        else {
            accessoryView = nil
        }
    }
}
