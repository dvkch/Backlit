//
//  PreviewCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift

class PreviewCell: UITableViewCell {

    override func awakeFromNib() {
        super.awakeFromNib()
        backgroundColor = .clear
        selectionStyle = .none
    }

    // MARK: Views
    @IBOutlet private var previewView: SanePreviewView!
    
    // MARK: Properties
    var device: Device? {
        didSet {
            previewView.device = device
        }
    }
    
    // MARK: Content
    func refresh() {
        previewView.refresh()
    }
    
    // MARK: Layout
    static func cellHeight(device: Device, width: CGFloat, maxHeight: CGFloat) -> CGFloat {
        let bestHeight = width / (device.previewImageRatio ?? (CGFloat(3) / 4))
        return min(max(bestHeight, width), maxHeight)
    }
}

