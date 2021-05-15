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

class OptionCell: TableViewCell {

    override func awakeFromNib() {
        super.awakeFromNib()
        
        titleLabel.isUserInteractionEnabled = true
        tooltipView = TooltipView(for: titleLabel, title: { [weak self] in
            if self?.valueControl != nil {
                return self?.option?.localizedDescr
            } else {
                return nil
            }
        })

        titleLabel.autoAdjustsFontSize = true
        valueLabel.autoAdjustsFontSize = true
        descrLabel.autoAdjustsFontSize = true
        
        setNeedsUpdateConstraints()
    }
    
    // MARK: Views
    private var tooltipView: TooltipView!
    @IBOutlet private var titleLabel: UILabel!
    @IBOutlet private var valueLabel: UILabel!
    @IBOutlet private var valueControl: UIView? {
        didSet {
            oldValue?.removeFromSuperview()
            if let valueControl = valueControl {
                valueControl.setContentCompressionResistancePriority(.required, for: .vertical)
                valueControl.setContentHuggingPriority(.required, for: .horizontal)
                contentView.addSubview(valueControl)
            }
            setNeedsUpdateConstraints()
        }
    }
    @IBOutlet private var descrLabel: UILabel!

    // MARK: Properties
    weak var delegate: DeviceOptionControllableDelegate?
    private var option: DeviceOption?
    private var prefKey: Preferences.Key?
    private var useMacOSLayout: Bool = false
    var showDescription: Bool = false {
        didSet {
            setNeedsUpdateConstraints()
        }
    }
    
    var hasControlToUpdateItsValue: Bool {
        return valueControl != nil
    }
    
    // MARK: Actions
    override var canBecomeFirstResponder: Bool {
        return hasControlToUpdateItsValue
    }
    
    override func pressesEnded(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        guard let press = presses.first else { return }

        if let slider = valueControl as? Slider, slider.isEnabled {
            if press.type == .leftArrow {
                return slider.decrement()
            }
            if press.type == .rightArrow {
                return slider.increment()
            }
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
        
        self.backgroundColor = UIDevice.isCatalyst ? .clear : backgroundColor

        titleLabel.textColor = normalTextColor
        valueLabel.textColor = normalTextColor
        descrLabel.textColor = descTextColor
        
        if let option = self.option {
            titleLabel.text = option.localizedTitle
            valueLabel.text = option.localizedValue
            descrLabel.text = option.localizedDescr

            #if targetEnvironment(macCatalyst)
            useMacOSLayout = true
            titleLabel.textAlignment = .right
            valueControl = nil
            option.updateDeviceOptionControl(using: self, filterDisabled: false, filterSingleOption: false)
            #endif
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
        #if targetEnvironment(macCatalyst)
        // Somehow on macCatayst (maybe even on iOS 13+) the cell sizing is slower than computing it ourselves (possibly because of menu rebuilding)
        return UITableView.automaticDimension
        #else
        sizingCell.updateWith(option: option)
        sizingCell.showDescription = showDescription
        return sizingCell.cellHeight(forWidth: width)
        #endif
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

        if option != nil && useMacOSLayout {
            let valueView: UIView = valueControl ?? valueLabel
            valueView.isHidden = false

            let hiddenValueView = valueControl != nil ? valueLabel : nil
            hiddenValueView?.snp.removeConstraints()
            hiddenValueView?.isHidden = true

            titleLabel.snp.remakeConstraints { (make) in
                make.top.left.equalTo(contentView.layoutMarginsGuide)
                make.width.equalTo(contentView.layoutMarginsGuide).multipliedBy(0.35)
            }

            valueView.snp.remakeConstraints { (make) in
                make.left.equalTo(titleLabel.snp.right).offset(20)
                make.top.right.equalTo(contentView.layoutMarginsGuide)
                make.height.equalTo(titleLabel).priority(760)
            }

            descrLabel.snp.remakeConstraints { (make) in
                make.top.equalTo(titleLabel.snp.bottom).offset(showDescription ? 20 : 0).priority(ConstraintPriority.required.value - 1)
                make.top.greaterThanOrEqualTo(titleLabel.snp.bottom).offset(showDescription ? 20 : 0)
                make.top.greaterThanOrEqualTo(valueView.snp.bottom).offset(showDescription ? 20 : 0)
                make.left.right.bottom.equalTo(contentView.layoutMarginsGuide)
                if !showDescription {
                    make.height.equalTo(0)
                }
            }
        }
        else if traitCollection.preferredContentSizeCategory.isAccessibilitySize {
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
        }
        else {
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

#if targetEnvironment(macCatalyst)
extension OptionCell: DeviceOptionControllable {
    func updateDeviceOptionControlForDisabledOption() {
        valueControl = nil
    }
    
    func updateDeviceOptionControlForList<T>(option: DeviceOptionTyped<T>, current: T, values: [T], supportsAuto: Bool) where T : CustomStringConvertible, T : Equatable {
        var dropdownOptions = values.map {
            CatalystDropdownValue(
                title: option.stringForValue($0, userFacing: true),
                value: NSValue(nonretainedObject: $0)
            )
        }
        
        if option.capabilities.contains(.automatic) {
            dropdownOptions.append(CatalystDropdownValue(title: "OPTION VALUE AUTO".localized, value: nil))
        }

        let selectedIndex = values.firstIndex(of: option.value) ?? -1
        valueControl = obtainCatalystPlugin().dropdown(options: dropdownOptions, selectedIndex: selectedIndex, disabled: option.disabledOrReadOnly, changed: { [weak self] selected in
            guard let self = self else { return }
            self.delegate?.deviceOptionControllable(self, willUpdate: option)
            if selected.value == nil {
                Sane.shared.updateOption(option, with: .auto, completion: self.optionUpdateCompletion(_:))
            } else {
                Sane.shared.updateOption(option, with: .value((selected.value as! NSValue).nonretainedObjectValue as! T), completion: self.optionUpdateCompletion(_:))
            }
        }).view
    }
    
    func updateDeviceOptionControlForRange<T>(option: DeviceOptionTyped<T>, current: Double, min: Double, max: Double, step: Double?, supportsAuto: Bool) where T : CustomStringConvertible, T : Numeric {
        let optionInt = option as? DeviceOptionInt
        let optionFixed = option as? DeviceOptionFixed
        
        guard optionInt != nil || optionFixed != nil else {
            fatalError("This method should only be used with numeric options")
        }

        let slider = Slider()
        slider.minimumValue = Float(min)
        slider.maximumValue = Float(max)
        slider.value = Float(current)
        slider.step = step.flatMap { Float($0) }
        slider.formatter = { optionInt?.stringForValue(Int($0), userFacing: true) ?? optionFixed!.stringForValue(Double($0), userFacing: true) }
        slider.isEnabled = !option.disabledOrReadOnly
        slider.changedBlock = { [weak self] value in
            guard let self = self else { return }
            if let optionInt = optionInt {
                self.delegate?.deviceOptionControllable(self, willUpdate: option)
                Sane.shared.updateOption(optionInt, with: .value(Int(value)), completion: self.optionUpdateCompletion(_:))
            }
            if let optionFixed = optionFixed {
                self.delegate?.deviceOptionControllable(self, willUpdate: option)
                Sane.shared.updateOption(optionFixed, with: .value(Double(value)), completion: self.optionUpdateCompletion(_:))
            }
        }
        slider.doubleTapBlock = { [weak self] in
            guard let self = self else { return }
            if supportsAuto {
                self.delegate?.deviceOptionControllable(self, willUpdate: option)
                Sane.shared.updateOption(option, with: .auto, completion: self.optionUpdateCompletion(_:))
            }
        }
        slider.translatesAutoresizingMaskIntoConstraints = false
        valueControl = slider
    }
    
    func updateDeviceOptionControlForField<T>(option: DeviceOptionTyped<T>, current: T, kind: DeviceOptionControllableFieldKind, supportsAuto: Bool) where T : CustomStringConvertible, T : Equatable {
        
        let field = UITextField()
        field.autocorrectionType = .no
        field.autocapitalizationType = .none
        field.text = option.value.description
        field.isEnabled = !option.disabledOrReadOnly
            
        switch kind {
        case .string:   field.keyboardType = .asciiCapable
        case .int:      field.keyboardType = .numberPad
        case .double:   field.keyboardType = .decimalPad
        }
        field.addTarget(self, action: #selector(deviceOptionTextFieldValueChanged), for: .primaryActionTriggered)

        valueControl = field
    }

    @objc private func deviceOptionTextFieldValueChanged(_ field: UITextField) {
        let stringValue = field.text ?? ""

        if let option = option as? DeviceOptionString {
            delegate?.deviceOptionControllable(self, willUpdate: option)
            Sane.shared.updateOption(option, with: .value(stringValue), completion: self.optionUpdateCompletion(_:))
        }
        if let option = option as? DeviceOptionInt {
            delegate?.deviceOptionControllable(self, willUpdate: option)
            Sane.shared.updateOption(option, with: .value(Int(stringValue) ?? option.value), completion: self.optionUpdateCompletion(_:))
        }
        if let option = option as? DeviceOptionFixed {
            delegate?.deviceOptionControllable(self, willUpdate: option)
            Sane.shared.updateOption(option, with: .value(Double(stringValue) ?? option.value), completion: self.optionUpdateCompletion(_:))
        }
    }
    
    func updateDeviceOptionControlForButton(option: DeviceOptionButton) {
        valueControl = obtainCatalystPlugin().button(title: "ACTION PRESS".localized) { [weak self] in
            guard let self = self else { return }
            self.delegate?.deviceOptionControllable(self, willUpdate: option)
            option.press(self.optionUpdateCompletion(_:))
        }.view
    }

    private func optionUpdateCompletion(_ error: Error?) {
        guard let option = option else { return }
        delegate?.deviceOptionControllable(self, didUpdate: option, error: error)
    }

}
#endif
