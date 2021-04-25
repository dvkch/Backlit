//
//  OptionCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SYKit
import SnapKit

class OptionCell: UITableViewCell {

    override func awakeFromNib() {
        super.awakeFromNib()
        
        titleLabel.autoAdjustsFontSize = true
        valueLabel.autoAdjustsFontSize = true
        descrLabel.autoAdjustsFontSize = true
        
        setNeedsUpdateConstraints()
    }
    
    // MARK: Views
    @IBOutlet private var titleLabel: UILabel!
    @IBOutlet private var valueLabel: UILabel!
    @IBOutlet private var descrLabel: UILabel!

    // MARK: Properties
    private var option: DeviceOption?
    private var prefKey: Preferences.Key?
    var showDescription: Bool = false {
        didSet {
            setNeedsUpdateConstraints()
        }
    }

    // MARK: Content
    func updateWith(option: DeviceOption) {
        self.option = option
        self.prefKey = nil
        updateTexts()
    }
    
    func updateWith(prefKey: Preferences.Key) {
        self.option = nil
        self.prefKey = prefKey
        updateTexts()
    }
    
    func updateWith(leftText: String, rightText: String) {
        self.option = nil
        self.prefKey = nil
        updateTexts()
        
        titleLabel.text = leftText
        valueLabel.text = rightText
        descrLabel.text = nil
        showDescription = false
    }
    
    private func updateTexts() {
        let disabled = option?.disabledOrReadOnly ?? false
        let backgroundColor = disabled ? UIColor.cellBackgroundAlt : UIColor.cellBackground
        let normalTextColor = disabled ? UIColor.disabledText : UIColor.normalText
        let descTextColor   = disabled ? UIColor.disabledText : UIColor.altText
        
        self.backgroundColor = backgroundColor
        titleLabel.textColor = normalTextColor
        valueLabel.textColor = normalTextColor
        descrLabel.textColor = descTextColor
        
        if let option = self.option {
            titleLabel.text = option.localizedTitle
            valueLabel.text = option.localizedValue
            descrLabel.text = option.localizedDescr
        }
        else if let prefKey = self.prefKey {
            titleLabel.text = prefKey.localizedTitle
            valueLabel.text = Preferences.shared[prefKey] ? "OPTION BOOL ON".localized : "OPTION BOOL OFF".localized
            descrLabel.text = prefKey.localizedDescription
        }
    }
    
    // MARK: Layout
    // cell heights are not proper because they depend on layoutMargins, that change according to safeAreaInsets. we only use those for estimated heights
    private static let sizingCell = UINib(nibName: "OptionCell", bundle: nil).instantiate(withOwner: nil, options: nil).first as! OptionCell
    
    static func cellHeight(option: DeviceOption, showDescription: Bool, width: CGFloat) -> CGFloat {
        sizingCell.updateWith(option: option)
        sizingCell.showDescription = showDescription
        return sizingCell.cellHeight(forWidth: width)
    }
    
    static func cellHeight(prefKey: Preferences.Key, showDescription: Bool, width: CGFloat) -> CGFloat {
        sizingCell.updateWith(prefKey: prefKey)
        sizingCell.showDescription = showDescription
        return sizingCell.cellHeight(forWidth: width)
    }
    
    static func cellHeight(leftText: String, rightText: String, width: CGFloat) -> CGFloat {
        sizingCell.updateWith(leftText: leftText, rightText: rightText)
        sizingCell.showDescription = false
        return sizingCell.cellHeight(forWidth: width)
    }
    
    private func cellHeight(forWidth: CGFloat) -> CGFloat {
        updateConstraintsIfNeeded()
        layoutIfNeeded()
        let size = contentView.systemLayoutSizeFitting(
            CGSize(width: forWidth, height: 8000),
            withHorizontalFittingPriority: .required,
            verticalFittingPriority: .fittingSizeLevel
        )
        return ceil(size.height)
    }
    
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        setNeedsUpdateConstraints()
    }
    
    override func updateConstraints() {
        descrLabel.isHidden = !showDescription

        if traitCollection.preferredContentSizeCategory.isAccessibilitySize {
            titleLabel.snp.remakeConstraints { (make) in
                make.top.left.right.equalTo(contentView.layoutMarginsGuide)
            }
            valueLabel.snp.remakeConstraints { (make) in
                make.top.equalTo(titleLabel.snp.bottom).offset(10)
                make.left.right.equalTo(contentView.layoutMarginsGuide)
            }
            descrLabel.snp.remakeConstraints { (make) in
                make.top.equalTo(valueLabel.snp.bottom).offset(showDescription ? 20 : 0)
                make.left.right.bottom.equalTo(contentView.layoutMarginsGuide)
                if !showDescription {
                    make.height.equalTo(0)
                }
            }
        } else {
            titleLabel.snp.remakeConstraints { (make) in
                make.top.left.equalTo(contentView.layoutMarginsGuide)
            }
            valueLabel.snp.remakeConstraints { (make) in
                make.left.equalTo(titleLabel.snp.right).offset(20)
                make.top.right.equalTo(contentView.layoutMarginsGuide)
            }
            descrLabel.snp.remakeConstraints { (make) in
                make.top.equalTo(titleLabel.snp.bottom).offset(showDescription ? 20 : 0).priority(ConstraintPriority.required.value - 1)
                make.top.greaterThanOrEqualTo(titleLabel.snp.bottom).offset(showDescription ? 20 : 0)
                make.top.greaterThanOrEqualTo(valueLabel.snp.bottom).offset(showDescription ? 20 : 0)
                make.left.right.bottom.equalTo(contentView.layoutMarginsGuide)
                if !showDescription {
                    make.height.equalTo(0)
                }
            }
        }
        
        super.updateConstraints()
        invalidateIntrinsicContentSize()
    }
}
