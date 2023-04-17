//
//  GalleryGridHeader.swift
//  SaneScanner
//
//  Created by syan on 12/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import SYKit

#if !targetEnvironment(macCatalyst)
class GalleryGridHeader: UICollectionReusableView {
    
    // MARK: Init
    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear

        background.gradientLayer.startPoint = .zero
        background.gradientLayer.endPoint = CGPoint(x: 0, y: 1)
        background.gradientLayer.locations = [0, 1]
        background.gradientLayer.colors = [UIColor.cellBackground.cgColor, UIColor.cellBackground.withAlphaComponent(0).cgColor]
        background.gradientLayer.cornerRadius = 8
        background.gradientLayer.maskedCorners = [.layerMinXMinYCorner, .layerMaxXMinYCorner]
        addSubview(background)
        background.snp.makeConstraints { make in
            make.top.centerX.equalToSuperview()
            make.left.equalToSuperview().offset(8)
            make.bottom.equalToSuperview().offset(40)
        }
        
        label.textColor = .normalText
        label.font = .preferredFont(forTextStyle: .body)
        label.adjustsFontForContentSizeCategory = true
        label.numberOfLines = 0
        addSubview(label)
        label.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(10)
            make.left.equalTo(background).offset(10)
            make.center.equalToSuperview()
        }
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    var group: GalleryGroup? {
        didSet {
            updateContent()
        }
    }
    
    // MARK: Views
    private let background = SYGradientView()
    private let label = UILabel()
    
    // MARK: Content
    private func updateContent() {
        guard let group, let firstItem = group.items.first else { return }
        var parts = [
            firstItem.creationDateString(includingTime: false, allowRelative: true)
        ]
        if group.items.count > 5 && !UIAccessibility.isVoiceOverRunning {
            parts.append("GALLERY ITEMS COUNT %d".localized(quantity: group.items.count))
        }
        label.text = parts.joined(separator: " – ")
    }
    
    // MARK: Sizing
    private static let sizingItem = GalleryGridHeader(frame: .zero)
    static func size(for group: GalleryGroup, in collectionView: UICollectionView) -> CGSize {
        // it needs to be part of the view hierarchy to inherit and update its `traitCollection.preferredContentSizeCategory`
        sizingItem.isHidden = true
        collectionView.addSubview(sizingItem)
        defer { sizingItem.removeFromSuperview() }

        sizingItem.traitCollectionDidChange(sizingItem.traitCollection)
        sizingItem.group = group

        let availableWidth = collectionView.bounds.inset(by: collectionView.adjustedContentInset).width
        return sizingItem.systemLayoutSizeFitting(
            CGSize(width: availableWidth, height: UIView.layoutFittingExpandedSize.height),
            withHorizontalFittingPriority: .required,
            verticalFittingPriority: .fittingSizeLevel
        )
    }
}
#endif
