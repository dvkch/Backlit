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
    func sanePreviewView(_ sanePreviewView: SanePreviewView, device: Device, tapped action: SanePreviewView.Action, progress: ((ScanProgress) -> ())?, completion: ((ScanResult) -> ())?)
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

        lineView.snp.makeConstraints { (make) in
            make.height.equalTo(1 / UIScreen.main.scale)
            make.left.right.equalToSuperview()
            make.bottom.equalTo(buttonsStackView.snp.top)
        }

        setNeedsUpdateConstraints()
        updateContent()
    }
    
    // MARK: Properties
    enum Action {
        case scan, preview
    }

    weak var delegate: SanePreviewViewDelegate?
    var device: Device? {
        didSet {
            setNeedsUpdateConstraints()
            refresh()
        }
    }
    var showScanButton: Bool = false {
        didSet {
            updateContent()
        }
    }
    
    private var currentAction: Action? {
        didSet {
            updateContent()
        }
    }
    private var progress: ScanProgress? {
        didSet {
            updateContent()
        }
    }
    
    // MARK: Views
    private let imageView = UIImageView()
    private let lineView = UIView()
    private let cropMask = CropMaskView()
    private let buttonsStackView = UIStackView()
    private let previewButton = ScanButton(type: .custom)
    private let scanButton = ScanButton(type: .custom)
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
    
    @objc private func buttonTap(_ sender: UIButton) {
        guard let device = device else { return }
        
        if case .cancelling = progress {
            // do nothing
            return
        }

        if progress != nil {
            delegate?.sanePreviewView(self, canceledScan: device)
            return
        }

        let action: Action = sender == previewButton ? .preview : .scan
        self.currentAction = action
        
        delegate?.sanePreviewView(self, device: device, tapped: action, progress: { [weak self] (progress) in
            guard self?.device == device else { return }
            self?.progress = progress
        }, completion: { [weak self] (result) in
            guard self?.device == device else { return }
            self?.progress = nil
            self?.currentAction = nil
            if case .success((let image, _)) = result {
                self?.updateImageIfValidPreview(image)
            }
        })
    }
    
    // MARK: Content
    private func updateContent() {
        lineView.isHidden = showScanButton

        scanButton.progress = currentAction == .scan ? progress : nil
        scanButton.isEnabled = currentAction != .preview
        scanButton.style = showScanButton ? .rounded : .cell
        scanButton.sy_isHidden = !showScanButton

        previewButton.progress = currentAction == .preview ? progress : nil
        previewButton.isEnabled = currentAction != .scan
        previewButton.style = showScanButton ? .rounded : .cell
        
        if case let .scanning(_, image) = progress {
            updateImageIfValidPreview(image)
        }
    }
    
    private func updateImageIfValidPreview(_ image: UIImage?) {
        guard device?.cropArea == device?.maxCropArea else { return }
        imageView.image = image
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