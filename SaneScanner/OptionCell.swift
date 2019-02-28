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

class OptionCell: UITableViewCell {

    override func awakeFromNib() {
        super.awakeFromNib()
        
        if #available(iOS 10.0, *) {
            labelTitle.adjustsFontForContentSizeCategory = true
            labelValue.adjustsFontForContentSizeCategory = true
            labelDescr.adjustsFontForContentSizeCategory = true
        }
    }
    
    // MARK: Views
    @IBOutlet private var labelTitle: UILabel!
    @IBOutlet private var labelValue: UILabel!
    @IBOutlet private var labelDescr: UILabel!
    @IBOutlet private var constraingLabelDescrTop: NSLayoutConstraint!
    private var constraingLabelDescrHeight: NSLayoutConstraint?

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
        
        labelTitle.text = leftText
        labelValue.text = rightText
        labelDescr.text = nil
        showDescription = false
    }
    
    private func updateTexts() {
        var backgroundColor = UIColor.white
        var normalTextColor = UIColor.darkText
        var descTextColor   = UIColor.gray
        
        if let option = self.option, option.disabledOrReadOnly {
            backgroundColor = UIColor(white: 0.98, alpha: 1)
            normalTextColor = UIColor.lightGray
            descTextColor   = UIColor.lightGray
        }
        
        self.backgroundColor = backgroundColor
        labelTitle.textColor = normalTextColor
        labelValue.textColor = normalTextColor
        labelDescr.textColor = descTextColor
        
        if let option = self.option {
            labelTitle.text = option.localizedTitle
            labelDescr.text = option.localizedDescr
            labelValue.text = option.localizedValue
        }
        else if let prefKey = self.prefKey {
            labelTitle.text = prefKey.localizedTitle
            labelValue.text = Preferences.shared[prefKey] ? "OPTION BOOL ON".localized : "OPTION BOOL OFF".localized
            labelDescr.text = prefKey.localizedDescription
        }
    }
    
    // MARK: Layout
    // cell heights are not proper because they depend on layoutMargins, that change according to safeAreaInsets. we only use those for estimated heights
    private static let sizingCell = UINib(nibName: "OptionCell", bundle: nil).instantiate(withOwner: nil, options: nil).first as! OptionCell
    
    static func cellHeight(option: DeviceOption, showDescription: Bool, width: CGFloat) -> CGFloat {
        sizingCell.updateWith(option: option)
        sizingCell.showDescription = showDescription
        return sizingCell.sy_cellHeight(forWidth: width)
    }
    
    static func cellHeight(prefKey: Preferences.Key, showDescription: Bool, width: CGFloat) -> CGFloat {
        sizingCell.updateWith(prefKey: prefKey)
        sizingCell.showDescription = showDescription
        return sizingCell.sy_cellHeight(forWidth: width)
    }
    
    static func cellHeight(leftText: String, rightText: String, width: CGFloat) -> CGFloat {
        sizingCell.updateWith(leftText: leftText, rightText: rightText)
        sizingCell.showDescription = false
        return sizingCell.sy_cellHeight(forWidth: width)
    }
    
    override func updateConstraints() {
        if showDescription, let c = constraingLabelDescrHeight {
            c.isActive = false
            constraingLabelDescrHeight = nil
        }
        
        if !showDescription, constraingLabelDescrHeight == nil {
            constraingLabelDescrHeight = labelDescr.heightAnchor.constraint(equalToConstant: 0)
            constraingLabelDescrHeight?.isActive = true
        }
        
        constraingLabelDescrTop.constant = showDescription ? 10 : 0
        
        super.updateConstraints()
    }
}
