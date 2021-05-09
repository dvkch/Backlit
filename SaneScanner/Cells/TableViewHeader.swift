//
//  TableViewHeader.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 09/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

class TableViewHeader: UITableViewHeaderFooterView {
    // MARK: Properties
    var text: String? {
        didSet {
            #if targetEnvironment(macCatalyst)
            label.text = text?.uppercased()
            #else
            textLabel?.text = text?.uppercased()
            #endif
            accessibilityLabel = text
        }
    }

    #if targetEnvironment(macCatalyst)
    // MARK: Init
    override init(reuseIdentifier: String?) {
        super.init(reuseIdentifier: reuseIdentifier)
        setup()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setup()
    }
    
    func setup() {
        container.backgroundColor = .altText
        contentView.addSubview(container)
        container.snp.makeConstraints { make in
            make.top.left.equalTo(contentView.layoutMarginsGuide)
            make.right.bottom.lessThanOrEqualTo(contentView.layoutMarginsGuide)
        }
        
        label.textColor = .background
        label.font = .boldSystemFont(ofSize: UIFont.preferredFont(forTextStyle: .caption1).pointSize)
        label.autoAdjustsFontSize = true
        label.setContentHuggingPriority(.required, for: .horizontal)
        label.setContentHuggingPriority(.required, for: .vertical)
        container.addSubview(label)
        label.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(2)
            make.left.equalToSuperview().offset(8)
            make.right.equalToSuperview().offset(-8)
            make.bottom.equalToSuperview().offset(-2).priority(.high)
        }
    }

    // MARK: Views
    private let container = UIView()
    private let label = UILabel()
    
    // MARK: Layout
    override func layoutSubviews() {
        super.layoutSubviews()
        container.layoutIfNeeded()
        container.layer.cornerRadius = container.bounds.height / 2
    }
    #endif
}
