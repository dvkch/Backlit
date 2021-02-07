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
    @IBOutlet private var previewView: SanePreviewView!
    
    // MARK: Properties
    weak var delegate: SanePreviewViewDelegate?
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
        let buttonHeight = NSAttributedString(string: "X", font: .preferredFont(forTextStyle: .body)).size().height * 2.5
        return min(max(bestImageHeight, width), maxImageHeight) + buttonHeight
    }
}

extension PreviewCell : SanePreviewViewDelegate {
    func sanePreviewView(_ sanePreviewView: SanePreviewView, tappedScan device: Device, updateBlock: ((UIImage?, Float, Bool) -> ())?) {
        delegate?.sanePreviewView(sanePreviewView, tappedScan: device, updateBlock: updateBlock)
    }
    
    func sanePreviewView(_ sanePreviewView: SanePreviewView, canceledScan device: Device) {
        delegate?.sanePreviewView(sanePreviewView, canceledScan: device)
    }
}
