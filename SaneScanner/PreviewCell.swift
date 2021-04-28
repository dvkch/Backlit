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
        let buttonHeight = ScanButton.preferredHeight
        return min(max(bestImageHeight, width), maxImageHeight) + buttonHeight
    }
}

extension PreviewCell : SanePreviewViewDelegate {
    func sanePreviewView(_ sanePreviewView: SanePreviewView, device: Device, tapped action: SanePreviewView.Action, progress: ((ScanProgress) -> ())?, completion: ((ScanResult) -> ())?) {
        delegate?.sanePreviewView(sanePreviewView, device: device, tapped: action, progress: progress, completion: completion)
    }
    
    func sanePreviewView(_ sanePreviewView: SanePreviewView, canceledScan device: Device) {
        delegate?.sanePreviewView(sanePreviewView, canceledScan: device)
    }
}
