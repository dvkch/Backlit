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
        isAccessibilityElement = true
        
        addTooltip { [weak self] in
            self?.item?.suggestedDescription(separator: "\n")
        }
        
        imageView.contentMode = .scaleAspectFit
        imageView.clipsToBounds = false
        imageView.layer.minificationFilter = .linear
        imageView.layer.magnificationFilter = .trilinear
        imageView.layer.shadowColor = UIColor.black.cgColor
        imageView.layer.shadowOffset = .zero
        imageView.layer.shadowRadius = 2
        contentView.addSubview(imageView)
        
        spinner.hidesWhenStopped = true
        contentView.addSubview(spinner)
        spinner.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        selectionBackgroundView.layer.cornerRadius = 22 / 2
        selectionBackgroundView.layer.masksToBounds = true
        contentView.addSubview(selectionBackgroundView)
        selectionBackgroundView.snp.makeConstraints { make in
            make.size.equalTo(22)
            make.bottom.right.equalTo(imageView).offset(-6)
        }
        
        selectionRingView.layer.borderColor = UIColor.white.cgColor
        selectionRingView.layer.borderWidth = 1
        selectionRingView.layer.cornerRadius = 22 / 2
        selectionRingView.layer.masksToBounds = false
        selectionRingView.layer.shadowColor = UIColor.black.cgColor
        selectionRingView.layer.shadowOffset = .zero
        selectionRingView.layer.shadowRadius = 3
        selectionRingView.layer.shadowOpacity = 0.8
        selectionRingView.layer.shouldRasterize = true
        selectionRingView.layer.rasterizationScale = UIScreen.main.scale
        contentView.addSubview(selectionRingView)
        selectionRingView.snp.makeConstraints { (make) in
            make.edges.equalTo(selectionBackgroundView)
        }
        
        updateSelectionStyle()
        setNeedsUpdateConstraints()
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    private(set) var item: GalleryItem?
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

    // MARK: Views
    private let imageView = UIImageView()
    private let spinner: UIActivityIndicatorView = {
        if #available(iOS 13.0, *) {
            return UIActivityIndicatorView(style: .medium)
        } else {
            return UIActivityIndicatorView(style: .white)
        }
    }()
    private let selectionRingView = UIView()
    private let selectionBackgroundView = UIView()

    // MARK: Content
    func update(item: GalleryItem, displayedOverTint: Bool) {
        self.item = item
        accessibilityLabel = item.suggestedAccessibilityLabel
        spinner.color = displayedOverTint ? .normalTextOnTint : .normalText
        updateThumbnail()
        
        setNeedsUpdateConstraints()
    }

    private func updateThumbnail() {
        imageView.image = nil
        updateStyle()

        let item = self.item
        GalleryManager.shared.thumbnail(for: item) { [weak self] in
            guard item == self?.item else { return }
            self?.imageView.image = $0
            self?.updateStyle()
        }
    }
    
    private func updateSelectionStyle() {
        selectionRingView.alpha = showSelectionIndicator ? 1 : 0
        selectionBackgroundView.alpha = selectionRingView.alpha
        selectionBackgroundView.backgroundColor = isSelected ? .tint : UIColor(white: 1, alpha: 0.7)
    }
    
    private func updateStyle() {
        if imageView.image == nil {
            spinner.startAnimating()
            imageView.backgroundColor = .backgroundAlt
            imageView.layer.shadowOpacity = 0
        }
        else {
            spinner.stopAnimating()
            imageView.backgroundColor = .clear
            imageView.layer.shadowOpacity = 0.6
        }
    }
    
    // MARK: Layout
    override func updateConstraints() {
        imageView.snp.remakeConstraints { (make) in
            if let item, let imageSize = GalleryManager.shared.imageSize(for: item) {
                make.width.equalTo(imageView.snp.height).multipliedBy(imageSize.width / imageSize.height)
            }
            make.top.left.equalToSuperview().priority(.high)
            make.top.left.greaterThanOrEqualToSuperview()
            make.center.equalToSuperview()
        }
        super.updateConstraints()
    }
    
    override func layoutSubviews() {
        super.layoutSubviews()
        contentView.layoutIfNeeded()
        imageView.layer.shadowPath = UIBezierPath(rect: imageView.bounds).cgPath
    }
    
    override func apply(_ layoutAttributes: UICollectionViewLayoutAttributes) {
        super.apply(layoutAttributes)

        // required for iOS 12, maybe lower versions as well (tested on iOS 12.4.1)
        // https://stackoverflow.com/a/57249637/1439489
        layer.zPosition = CGFloat(layoutAttributes.zIndex)
    }
}
