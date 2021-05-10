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
import SYKit

protocol SanePreviewViewDelegate: NSObjectProtocol {
    func sanePreviewView(_ sanePreviewView: SanePreviewView, device: Device, tapped action: ScanOperation, progress: ((ScanProgress) -> ())?, completion: ((ScanResult) -> ())?)
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
        
        progressMaskContainer.isUserInteractionEnabled = false
        addSubview(progressMaskContainer)

        progressMask.gradientLayer.type = .axial
        progressMask.isUserInteractionEnabled = false
        progressMaskContainer.addSubview(progressMask)
        
        buttonsStackView.spacing = contentInsets
        buttonsStackView.axis = .horizontal
        buttonsStackView.distribution = .fillEqually
        buttonsStackView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(buttonsStackView)
        
        previewButton.kind = .preview
        previewButton.addTarget(self, action: #selector(self.buttonTap), for: .touchUpInside)
        buttonsStackView.addArrangedSubview(previewButton)

        scanButton.kind = .scan
        scanButton.addTarget(self, action: #selector(self.buttonTap), for: .touchUpInside)
        buttonsStackView.addArrangedSubview(scanButton)

        imageView.snp.makeConstraints { (make) in
            make.top.equalTo(contentInsets)
            make.centerX.equalToSuperview()
            make.left.greaterThanOrEqualTo(contentInsets)
            make.right.lessThanOrEqualTo(-contentInsets)
            make.bottom.lessThanOrEqualTo(buttonsStackView.snp.top).offset(-contentInsets)
            make.bottom.equalTo(buttonsStackView.snp.top).offset(-contentInsets).priority(.high)
        }

        cropMask.snp.makeConstraints { (make) in
            make.edges.equalTo(imageView)
        }
        
        progressMaskContainer.snp.makeConstraints { make in
            make.edges.equalTo(imageView)
        }

        lineView.snp.makeConstraints { (make) in
            make.height.equalTo(1 / UIScreen.main.scale)
            make.left.right.equalToSuperview()
            make.bottom.equalTo(buttonsStackView.snp.top)
        }

        setNeedsUpdateConstraints()
        refresh()
    }
    
    // MARK: Properties
    weak var delegate: SanePreviewViewDelegate?
    var device: Device? {
        didSet {
            setNeedsUpdateConstraints()
            refresh()
        }
    }
    var showScanButton: Bool = false {
        didSet {
            refresh()
        }
    }
    
    // MARK: Views
    private let imageView = UIImageView()
    private let lineView = UIView()
    private let cropMask = CropMaskView()
    private let progressMaskContainer = UIView()
    private let progressMask = SYGradientView()
    private let buttonsStackView = UIStackView()
    private let previewButton = ScanButton(type: .custom)
    private let scanButton = ScanButton(type: .custom)
    private var ratioConstraint: NSLayoutConstraint?

    // MARK: Actions
    @objc private func buttonTap(_ sender: UIButton) {
        guard let device = device else { return }
        
        if case .cancelling = device.currentOperation?.progress {
            // do nothing
            return
        }

        if device.currentOperation?.progress != nil {
            delegate?.sanePreviewView(self, canceledScan: device)
            return
        }

        let action: ScanOperation = sender == previewButton ? .preview : .scan
        
        delegate?.sanePreviewView(self, device: device, tapped: action, progress: { [weak self] (progress) in
            self?.refresh()
        }, completion: { [weak self] (result) in
            self?.refresh()
        })
    }
    
    // MARK: Content
    func refresh() {
        lineView.sy_isHidden = showScanButton

        scanButton.progress = device?.currentOperation?.operation == .scan ? device?.currentOperation?.progress : nil
        scanButton.isEnabled = device?.currentOperation?.operation != .preview
        scanButton.style = showScanButton ? .rounded : .cell
        scanButton.sy_isHidden = !showScanButton

        previewButton.progress = device?.currentOperation?.operation == .preview ? device?.currentOperation?.progress : nil
        previewButton.isEnabled = device?.currentOperation?.operation != .scan
        previewButton.style = showScanButton ? .rounded : .cell
        
        updateImageAndCrop()

        #if targetEnvironment(simulator)
        prepareForSnapshotting()
        #endif
    }
    
    private func updateImageAndCrop() {
        guard let device = device else {
            cropMask.isHidden = true
            imageView.image = nil
            return
        }
        
        cropMask.sy_isHidden = !device.canCrop
        cropMask.setCropArea(device.cropArea, maxCropArea: device.maxCropArea)
        cropMask.isEnabled = device.currentOperation == nil

        if device.currentOperation?.operation == .preview, let progress = device.currentOperation?.progress, case .scanning(_, let image, _) = progress, image != nil {
            imageView.image = image
        }
        else {
            imageView.image = device.lastPreviewImage
        }
    }
    
    // MARK: Layout
    private let contentInsets: CGFloat = 15
    
    override func updateConstraints() {
        let ratio = device?.previewImageRatio ?? (CGFloat(3) / 4)
        
        if let ratioConstraint = ratioConstraint {
            imageView.removeConstraint(ratioConstraint)
        }
        
        ratioConstraint = imageView.widthAnchor.constraint(equalTo: imageView.heightAnchor, multiplier: ratio)
        ratioConstraint?.isActive = true

        let stackViewMargins = showScanButton ? contentInsets : 0
        buttonsStackView.snp.remakeConstraints { (make) in
            make.left.equalToSuperview().offset(stackViewMargins)
            make.right.bottom.equalToSuperview().offset(-stackViewMargins)
        }

        super.updateConstraints()
    }
    
    override func layoutSubviews() {
        buttonsStackView.axis = bounds.width > 400 ? .horizontal : .vertical
        super.layoutSubviews()
    }
}

extension SanePreviewView {
    private func prepareForSnapshotting() {
        guard let device = device else { return }

        let snapshotType = SnapshotKind.fromLaunchOptions
        if snapshotType == .devicePreview || snapshotType == .deviceOptions || snapshotType == .deviceOptionPopup {
            imageView.image = SnapshotKind.snapshotTestScanImagePath.flatMap { UIImage(contentsOfFile: $0) }

            let cropAreaPercent = CGRect(x: 0.1, y: 0.2, width: 0.8, height: 0.6)
            var cropArea = CGRect()
            cropArea.origin.x = device.maxCropArea.origin.x + device.maxCropArea.width * cropAreaPercent.origin.x
            cropArea.origin.y = device.maxCropArea.origin.y + device.maxCropArea.height * cropAreaPercent.origin.y
            cropArea.size.width = device.maxCropArea.width * cropAreaPercent.width
            cropArea.size.height = device.maxCropArea.height * cropAreaPercent.height

            cropMask.setCropArea(device.maxCropArea.intersection(cropArea), maxCropArea: device.maxCropArea)
        }
    }
}
