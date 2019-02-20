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
import SVProgressHUD

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
        
        button.setTitle("DEVICE BUTTON UPDATE PREVIEW".localized, for: .normal)
        button.setTitleColor(.black, for: .normal)
        button.backgroundColor = .white
        button.titleLabel?.font = .systemFont(ofSize: 17)
        if #available(iOS 10.0, *) {
            button.titleLabel?.adjustsFontForContentSizeCategory = true
        }
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
            make.height.equalTo(40)
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
    var device: Device? {
        didSet {
            setNeedsUpdateConstraints()
            refresh()
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
        SVProgressHUD.show(withStatus: "PREVIEWING".localized)
        Sane.shared.preview(device: device, progress: { [weak self] (progress, image) in
            self?.imageView.image = image
            SVProgressHUD.showProgress(progress)
        }, completion: { [weak self] (image, error) in
            if let error = error {
                SVProgressHUD.showError(withStatus: error.localizedDescription)
            }
            else {
                SVProgressHUD.dismiss()
            }
            self?.refresh()
        })
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
        
        super.updateConstraints()
    }
}
