//
//  SanePreviewView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SnapKit

protocol SanePreviewViewDelegate: NSObjectProtocol {
    func sanePreviewView(_ sanePreviewView: SanePreviewView, tappedScan device: Device, updateBlock: ((UIImage?, Float, Bool) -> ())?)
    func sanePreviewView(_ sanePreviewView: SanePreviewView, canceledScan device: Device)
}

class SanePreviewView: UIView {

    // MARK: Init
    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    private func setup() {
        backgroundColor = .clear
        
        imageView.contentMode = .scaleAspectFit
        imageView.backgroundColor = .white
        imageView.layer.shadowColor = UIColor.black.cgColor
        imageView.layer.shadowOffset = .zero
        imageView.layer.shadowRadius = 5
        imageView.layer.shadowOpacity = 0.3
        imageView.layer.shouldRasterize = true
        imageView.layer.rasterizationScale = UIScreen.main.scale
        addSubview(imageView)
        
        lineView.backgroundColor = UITableView(frame: .zero, style: .grouped).separatorColor
        addSubview(lineView)
        
        cropMask.cropAreaDidChangeBlock = { [weak self] (newCropArea) in
            self?.device?.cropArea = newCropArea
        }
        addSubview(cropMask)
        
        button.setTitleColor(.normalText, for: .normal)
        button.backgroundColor = .cellBackground
        button.titleLabel?.font = .preferredFont(forTextStyle: .body)
        button.titleLabel?.autoAdjustsFontSize = true
        button.titleLabel?.numberOfLines = 2
        button.addTarget(self, action: #selector(self.buttonTap), for: .touchUpInside)
        addSubview(button)
        updateButton()
        
        cropMask.snp.makeConstraints { (make) in
            make.edges.equalTo(imageView)
        }

        lineView.snp.makeConstraints { (make) in
            make.height.equalTo(1 / UIScreen.main.scale)
            make.left.right.equalToSuperview()
            make.bottom.equalTo(button.snp.top)
        }
        
        button.snp.makeConstraints { (make) in
            make.height.equalTo(50)
            make.left.right.bottom.equalToSuperview()
        }
        
        imageView.snp.makeConstraints { (make) in
            make.top.equalTo(margin)
            make.centerX.equalToSuperview()
            make.left.greaterThanOrEqualTo(margin)
            make.right.lessThanOrEqualTo(-margin)
            make.bottom.lessThanOrEqualTo(button.snp.top).offset(-margin)
            make.bottom.equalTo(button.snp.top).offset(-margin).priority(.high)
        }
        
        setNeedsUpdateConstraints()
    }
    
    // MARK: Properties
    weak var delegate: SanePreviewViewDelegate?
    var device: Device? {
        didSet {
            setNeedsUpdateConstraints()
            refresh()
        }
    }
    
    enum Status {
        case inactive
        case previewing(Float)
        case canceling
    }
    private var status: Status = .inactive {
        didSet {
            updateButton()
        }
    }
    
    // MARK: Views
    private let imageView = UIImageView()
    private let lineView = UIView()
    private let cropMask = CropMaskView()
    private let button = UIButton(type: .custom)
    private var ratioConstraint: NSLayoutConstraint?

    // MARK: Actions
    func refresh() {
        guard let device = device else {
            cropMask.isHidden = true
            imageView.image = nil
            return
        }
        
        cropMask.isHidden = !device.canCrop
        cropMask.setCropArea(device.cropArea, maxCropArea: device.maxCropArea)
        imageView.image = device.lastPreviewImage
    }
    
    @objc private func buttonTap() {
        guard let device = device else { return }
        
        switch status {
        case .inactive:
            delegate?.sanePreviewView(self, tappedScan: device, updateBlock: { [weak self] image, progress, finished in
                guard self?.device == device else { return }
                self?.imageView.image = image
                if finished {
                    self?.status = .inactive
                    self?.refresh()
                } else {
                    self?.status = .previewing(progress)
                }
            })
            
        case .previewing:
            delegate?.sanePreviewView(self, canceledScan: device)
            self.status = .canceling
        
        case .canceling: break
        }
    }
    
    // MARK: Content
    private func updateButton() {
        switch status {
        case .inactive:
            button.setAttributedTitle(nil, for: .normal)
            button.setTitle("DEVICE BUTTON UPDATE PREVIEW".localized, for: .normal)

        case .previewing(let float):
            let title = NSAttributedString(string: String(format: "PREVIEWING %f".localized, float * 100), font: button.titleLabel?.font, color: .normalText)
            let subtitle = NSAttributedString(string: "ACTION HINT TAP TO CANCEL".localized, font: button.titleLabel?.font, color: .altText)
            let fullTitle: NSAttributedString = [title, subtitle].concat(separator: "\n").setParagraphStyle(alignment: .center, lineSpacing: 0, paragraphSpacing: 0)
            button.setAttributedTitle(fullTitle, for: .normal)

        case .canceling:
            button.setAttributedTitle(nil, for: .normal)
            button.setTitle("CANCELLING".localized, for: .normal)
        }
    }
    
    // MARK: Layout
    private let margin = CGFloat(15)
    
    override func updateConstraints() {
        let ratio = device?.previewImageRatio ?? (CGFloat(3) / 4)
        
        if let ratioConstraint = ratioConstraint {
            imageView.removeConstraint(ratioConstraint)
        }
        
        ratioConstraint = imageView.widthAnchor.constraint(equalTo: imageView.heightAnchor, multiplier: ratio)
        ratioConstraint?.isActive = true

        button.snp.remakeConstraints { (make) in
            make.height.equalTo(NSAttributedString(string: "X", font: button.titleLabel?.font).size().height * 2.5)
            make.left.right.bottom.equalToSuperview()
        }

        super.updateConstraints()
    }
    
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        setNeedsUpdateConstraints()
    }
}
