//
//  PreviewView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SnapKit
import SYKit

protocol PreviewViewDelegate: NSObjectProtocol {
    func previewView(_ previewView: PreviewView, device: Device, tapped action: ScanOperation)
    func previewView(_ previewView: PreviewView, canceledScan device: Device)
}

class PreviewView: UIView {

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
        
        addSubview(progressMask)

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
        
        progressMask.snp.makeConstraints { make in
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
    weak var delegate: PreviewViewDelegate?
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
    var cropAreaInViewBounds: CGRect {
        return convert(cropMask.cropAreaInViewBounds, from: cropMask)
    }
    
    // MARK: Views
    private let imageView = UIImageView()
    private let lineView = UIView()
    private let cropMask = CropMaskView()
    private let progressMask = ProgressMaskView()
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
            delegate?.previewView(self, canceledScan: device)
            return
        }

        let action: ScanOperation = sender == previewButton ? .preview : .scan
        delegate?.previewView(self, device: device, tapped: action)
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
        updateProgressMask()

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
        cropMask.isEnabled = device.canCrop && device.currentOperation == nil
        cropMask.setCropArea(device.cropArea, maxCropArea: device.maxCropArea)

        if device.currentOperation?.operation == .preview, let progress = device.currentOperation?.progress, case .scanning(_, _, let image, _) = progress, image != nil {
            imageView.image = image
        }
        else {
            imageView.image = device.lastPreviewImage
        }
    }
    
    private func updateProgressMask() {
        guard let device = device, device.currentOperation?.operation == .scan, case let .scanning(progress, _, _, parameters) = device.currentOperation?.progress else {
            progressMask.cropAreaPercent = nil
            progressMask.progress = nil
            return
        }

        progressMask.cropAreaPercent = parameters.cropArea.asPercents(of: device.maxCropArea)
        progressMask.progress = progress
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

extension PreviewView {
    private func prepareForSnapshotting() {
        guard let device = device else { return }

        if Snapshot.isSnapshotting, let testImagePath = Snapshot.snapshotTestScanImagePath {
            imageView.image = UIImage(contentsOfFile: testImagePath)

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
