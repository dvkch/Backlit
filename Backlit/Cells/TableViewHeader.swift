//
//  TableViewHeader.swift
//  Backlit
//
//  Created by syan on 09/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

class TableViewHeader: UITableViewHeaderFooterView {

    // MARK: Init
    override init(reuseIdentifier: String?) {
        super.init(reuseIdentifier: reuseIdentifier)
        setup()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setup()
    }

    // MARK: Properties
    var topMargin: CGFloat = 0 {
        didSet {
            topMarginConstraint?.constant = topMargin
            setNeedsUpdateConstraints()
        }
    }
    var text: String? {
        didSet {
            label.text = text?.uppercased(with: .current)
            accessibilityLabel = text
        }
    }
    
    func setup() {
        #if !targetEnvironment(macCatalyst)
        label.textColor = .altText
        label.font = .preferredFont(forTextStyle: .caption1)
        label.adjustsFontForContentSizeCategory = true
        label.setContentHuggingPriority(.required, for: .horizontal)
        label.setContentHuggingPriority(.required, for: .vertical)
        contentView.addSubview(label)
        label.snp.makeConstraints { make in
            topMarginConstraint = make.top.equalTo(contentView.layoutMarginsGuide).priority(.high).constraint.layoutConstraints.first
            make.left.equalTo(contentView.layoutMarginsGuide)
            make.right.greaterThanOrEqualTo(contentView.layoutMarginsGuide)
            make.bottom.equalTo(contentView.layoutMarginsGuide)
        }
        #else
        container.backgroundColor = .altText
        contentView.addSubview(container)
        container.snp.makeConstraints { make in
            topMarginConstraint = make.top.equalTo(contentView.layoutMarginsGuide).constraint.layoutConstraints.first
            make.left.equalTo(contentView.layoutMarginsGuide)
            make.right.lessThanOrEqualTo(contentView.layoutMarginsGuide).priority(.high)
            make.bottom.equalTo(contentView.layoutMarginsGuide).priority(.high)
        }
        
        label.textColor = .background
        label.font = .boldSystemFont(ofSize: UIFont.preferredFont(forTextStyle: .caption1).pointSize)
        label.adjustsFontForContentSizeCategory = true
        label.setContentHuggingPriority(.required, for: .horizontal)
        label.setContentHuggingPriority(.required, for: .vertical)
        container.addSubview(label)
        label.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(2)
            make.left.equalToSuperview().offset(8)
            make.right.equalToSuperview().offset(-8)
            make.bottom.equalToSuperview().offset(-2).priority(.high)
        }
        #endif
    }

    // MARK: Views
    private var topMarginConstraint: NSLayoutConstraint?
    #if targetEnvironment(macCatalyst)
    private let container = UIView()
    #endif
    private let label = UILabel()

    #if targetEnvironment(macCatalyst)
    // MARK: Layout
    override func layoutSubviews() {
        super.layoutSubviews()
        container.layoutIfNeeded()
        container.layer.cornerRadius = container.bounds.height / 2
    }
    #endif
    
    // MARK: Sizing
    private static let sizingItem = TableViewHeader(frame: CGRect(x: 0, y: 0, width: 100, height: 100))
    static func height(for text: String, topMargin: CGFloat, at section: Int, in tableView: UITableView) -> CGFloat {
        let availableWidth = tableView.bounds.inset(by: tableView.adjustedContentInset).width

        // it needs to be part of the view hierarchy to inherit and update its `traitCollection.preferredContentSizeCategory`
        sizingItem.isHidden = true
        tableView.addSubview(sizingItem)
        defer { sizingItem.removeFromSuperview() }

        sizingItem.traitCollectionDidChange(sizingItem.traitCollection)
        sizingItem.topMargin = topMargin
        sizingItem.text = text

        let size = sizingItem.systemLayoutSizeFitting(
            CGSize(width: availableWidth, height: UIView.layoutFittingExpandedSize.height),
            withHorizontalFittingPriority: .required,
            verticalFittingPriority: .fittingSizeLevel
        )
        return size.height
    }
}
