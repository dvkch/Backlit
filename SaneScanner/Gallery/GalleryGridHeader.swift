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
    var items: [GalleryItem] = [] {
        didSet {
            updateContent()
        }
    }
    
    // MARK: Views
    private let background = SYGradientView()
    private let label = UILabel()
    
    // MARK: Content
    private static let dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateStyle = .long
        formatter.timeStyle = .none
        formatter.locale = .autoupdatingCurrent
        formatter.timeZone = .autoupdatingCurrent
        formatter.formattingContext = .beginningOfSentence
        return formatter
    }()

    private static let relativeDateFormatter: Formatter? = {
        if #available(iOS 13.0, *) {
            let formatter = RelativeDateTimeFormatter()
            formatter.unitsStyle = .full
            formatter.locale = .autoupdatingCurrent
            formatter.calendar = .autoupdatingCurrent
            formatter.dateTimeStyle = .named
            formatter.formattingContext = .beginningOfSentence
            return formatter
        } else {
            return nil
        }
    }()
    
    private var dateString: String? {
        guard let date = items.first?.creationDate else { return nil }

        if Date().timeIntervalSince(date) < 7 * 24 * 3600,
           #available(iOS 13.0, *), let formatter = type(of: self).relativeDateFormatter as? RelativeDateTimeFormatter
        {
            let components = Calendar.current.dateComponents(Set([Calendar.Component.day]), from: Date(), to: date)
            return formatter.localizedString(from: components)
        }
        else {
            return type(of: self).dateFormatter.string(from: date)
        }
    }

    private func updateContent() {
        var parts = [dateString]
        if items.count > 5 && !UIAccessibility.isVoiceOverRunning {
            parts.append("GALLERY ITEMS COUNT %d".localized(quantity: items.count))
        }
        label.text = parts.removingNils().joined(separator: " – ")
    }
    
    // MARK: Sizing
    private static let sizingItem = GalleryGridHeader(frame: .zero)
    static func size(for items: [GalleryItem], in collectionView: UICollectionView) -> CGSize {
        // it needs to be part of the view hierarchy to inherit and update its `traitCollection.preferredContentSizeCategory`
        sizingItem.isHidden = true
        collectionView.addSubview(sizingItem)
        defer { sizingItem.removeFromSuperview() }

        sizingItem.traitCollectionDidChange(sizingItem.traitCollection)
        sizingItem.items = items

        let availableWidth = collectionView.bounds.inset(by: collectionView.adjustedContentInset).width
        return sizingItem.systemLayoutSizeFitting(
            CGSize(width: availableWidth, height: UIView.layoutFittingExpandedSize.height),
            withHorizontalFittingPriority: .required,
            verticalFittingPriority: .fittingSizeLevel
        )
    }
}
#endif
