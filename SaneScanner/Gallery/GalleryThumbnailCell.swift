//
//  GalleryThumbnailCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class GalleryThumbnailCell: UICollectionViewCell {

    override init(frame: CGRect) {
        super.init(frame: frame)
        
        imageView.contentMode = .scaleAspectFill
        imageView.clipsToBounds = true
        imageView.layer.minificationFilter = .trilinear
        contentView.addSubview(imageView)
        imageView.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        spinner.hidesWhenStopped = true
        contentView.addSubview(spinner)
        spinner.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        selectionView.layer.borderColor = UIColor.white.cgColor
        selectionView.layer.borderWidth = 1
        selectionView.layer.cornerRadius = 22 / 2
        selectionView.layer.masksToBounds = true
        contentView.addSubview(selectionView)
        selectionView.snp.makeConstraints { (make) in
            make.size.equalTo(22)
            make.bottom.right.equalTo(-8)
        }
        
        updateSelectionStyle()
        
        GalleryManager.shared.addDelegate(self)
        
        if #available(iOS 13.0, *) {
            tooltipGesture = UIHoverGestureRecognizer(target: self, action: #selector(hoverGestureRecognized(_:)))
            addGestureRecognizer(tooltipGesture)
        }
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    private var item: GalleryItem?
    private var mode: Mode = .gallery
    var showSelectionIndicator: Bool = false {
        didSet {
            updateSelectionStyle()
        }
    }
    override var isSelected: Bool {
        didSet {
            updateSelectionStyle()
        }
    }
    
    enum Mode {
        case gallery, toolbar
    }

    private var hoverTimer: Timer? {
        didSet {
            oldValue?.invalidate()
        }
    }
    
    // MARK: Views
    private let imageView = UIImageView()
    private let spinner: UIActivityIndicatorView = {
        if #available(iOS 13.0, *) {
            return UIActivityIndicatorView(style: .medium)
        } else {
            return UIActivityIndicatorView(style: .white)
        }
    }()
    private let selectionView = UIView()
    private var tooltipGesture: UIGestureRecognizer!
    private let tooltipView = TooltipView()
    
    // MARK: Actions
    @objc private func hoverGestureRecognized(_ gesture: UIGestureRecognizer) {
        if gesture.state == .began {
            hoverTimer = Timer(timeInterval: 0.7, target: self, selector: #selector(showTooltip), userInfo: nil, repeats: false)
            RunLoop.main.add(hoverTimer!, forMode: .common)
        }
        if gesture.state == .ended || gesture.state == .cancelled {
            hoverTimer?.invalidate()
            hideTooltip()
        }
    }
    
    @objc private func showTooltip() {
        guard let item = item else { return }

        let text = GalleryManager.shared.dateString(for: item) ?? ""
        var location = tooltipGesture.location(in: self)
        location.y += 15 // approximate cursor height
        tooltipView.show(text: text, from: self, location: location)
    }
    
    func hideTooltip() {
        tooltipView.dismiss()
    }

    // MARK: Content
    func update(item: GalleryItem, mode: Mode, spinnerColor: UIColor) {
        spinner.color = spinnerColor
        self.item = item
        self.mode = mode
        
        updateThumbnail(nil)
    }

    private func updateThumbnail(_ thumbnail: UIImage?) {
        imageView.image = thumbnail ?? GalleryManager.shared.thumbnail(for: item)
        updateStyle()
    }
    
    private func updateSelectionStyle() {
        selectionView.alpha = showSelectionIndicator ? 1 : 0
        selectionView.backgroundColor = isSelected ? .tint : UIColor(white: 1, alpha: 0.7)
    }
    
    private func updateStyle() {
        if imageView.image != nil {
            spinner.stopAnimating()
            imageView.backgroundColor = .white
        }
        else {
            spinner.startAnimating()
            imageView.backgroundColor = .lightGray
        }

        switch mode {
        case .gallery:
            contentView.layer.shadowColor = nil
            contentView.layer.shouldRasterize = false
            if imageView.image != nil {
                imageView.backgroundColor = .white
            }
            else {
                imageView.backgroundColor = .lightGray
            }

        case .toolbar:
            contentView.layer.shadowColor = UIColor.black.cgColor
            contentView.layer.shadowOffset = .zero
            contentView.layer.shadowRadius = 2
            contentView.layer.rasterizationScale = UIScreen.main.scale
            
            if imageView.image != nil {
                contentView.layer.shadowOpacity = 0.6
                contentView.layer.shouldRasterize = true
            } else {
                contentView.layer.shadowOpacity = 0
                contentView.layer.shouldRasterize = false
            }
        }
    }
}

extension GalleryThumbnailCell : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) { }
    
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) {
        guard item == self.item else { return }
        updateThumbnail(thumbnail)
    }
}

