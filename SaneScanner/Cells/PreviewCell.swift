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
        previewView.delegate = self
    }

    // MARK: Views
    @IBOutlet private var previewView: PreviewView!
    
    // MARK: Properties
    weak var delegate: PreviewViewDelegate?
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
    static func cellHeight(device: Device, width: CGFloat, maxImageHeight: CGFloat) -> CGFloat {
        let imageRatio = device.previewImageRatio ?? (CGFloat(3) / 4)
        let bestImageHeight = width / imageRatio
        let buttonHeight = ScanButton.preferredHeight
        return min(max(bestImageHeight, width), maxImageHeight) + buttonHeight
    }
}

extension PreviewCell : PreviewViewDelegate {
    func previewView(_ previewView: PreviewView, device: Device, tapped action: ScanOperation) {
        delegate?.previewView(previewView, device: device, tapped: action)
    }
    
    func previewView(_ previewView: PreviewView, canceledScan device: Device) {
        delegate?.previewView(previewView, canceledScan: device)
    }
}
