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
    func sanePreviewView(_ sanePreviewView: SanePreviewView, tappedScan device: Device, progress: ((ScanProgress) -> ())?, completion: ((ScanResult) -> ())?)
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
        updateContent()
    }
    
    // MARK: Properties
    weak var delegate: SanePreviewViewDelegate?
    var device: Device? {
        didSet {
            setNeedsUpdateConstraints()
            refresh()
        }
    }
    
    private var previewProgress: ScanProgress? {
        didSet {
            updateContent()
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
        
        if case .cancelling = previewProgress {
            // do nothing
            return
        }

        if previewProgress != nil {
            delegate?.sanePreviewView(self, canceledScan: device)
            return
        }

        
        delegate?.sanePreviewView(self, tappedScan: device, progress: { [weak self] (progress) in
            guard self?.device == device else { return }
            self?.previewProgress = progress
        }, completion: { [weak self] (result) in
            guard self?.device == device else { return }
            self?.previewProgress = nil
            if case .success((let image, _)) = result {
                self?.imageView.image = image
            }
        })
    }
    
    // MARK: Content
    private func updateContent() {
        button.updateTitle(progress: previewProgress, isPreview: true)
        if case let .scanning(_, image) = previewProgress {
            imageView.image = image
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
