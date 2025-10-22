//
//  OptionCell.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 02/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit
import SnapKit
import SaneSwift

class OptionCell: TableViewCell {

    override func awakeFromNib() {
        super.awakeFromNib()
        
        titleTooltip.accessibilityElementsHidden = true
        titleLabel.addSubview(titleTooltip)
        titleTooltip.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }

        titleLabel.adjustsFontForContentSizeCategory = true
        valueLabel.adjustsFontForContentSizeCategory = true
        descrLabel.adjustsFontForContentSizeCategory = true
        
        setNeedsUpdateConstraints()
    }
    
    // MARK: Views
    private let titleTooltip = UIControl()
    @IBOutlet private var titleLabel: UILabel!
    @IBOutlet private var valueLabel: UILabel!
    private var valueControl: UIControl? {
        didSet {
            oldValue?.removeFromSuperview()
            if let valueControl = valueControl {
                valueControl.setContentCompressionResistancePriority(.required, for: .vertical)
                valueControl.setContentCompressionResistancePriority(.required, for: .horizontal)
                valueControl.setContentHuggingPriority(.required, for: .horizontal)
                contentView.addSubview(valueControl)
            }
            setNeedsUpdateConstraints()
        }
    }
    @IBOutlet private var descrLabel: UILabel!

    // MARK: Properties
    weak var delegate: DeviceOptionControllableDelegate?
    private(set) var option: DeviceOption?
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

        super.pressesEnded(presses, with: event)
    }
    
    #if DEBUG
    func triggerValueControl() {
        if let button = valueControl as? UIButton, let interaction = button.contextMenuInteraction {
            // _presentMenuAtLocation:
            interaction.perform(Selector("_presentMe" + ":noitacoLtAun".reversed()), with: CGPoint.zero)
        }
        else {
            valueControl?.sendActions(for: .primaryActionTriggered)
        }
    }
    #endif
    
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
    
    func updateWith(leftText: String, rightText: String, description: String? = nil) {
        self.option = nil
        self.prefKey = nil
        updateTexts()
        
        titleLabel.text = leftText
        valueLabel.text = rightText
        descrLabel.text = description
        showDescription = description != nil
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
            accessibilityIdentifier = option.identifier

            #if targetEnvironment(macCatalyst)
            useMacOSLayout = true
            titleLabel.textAlignment = .right
            #endif
            valueControl = nil
            option.updateDeviceOptionControl(using: self, filterDisabled: false, filterSingleOption: false)
        }
        else if let prefKey = self.prefKey {
            titleLabel.text = prefKey.localizedTitle
            valueLabel.text = Preferences.shared[prefKey].description
            descrLabel.text = prefKey.localizedDescription
            accessibilityIdentifier = prefKey.rawValue

            if let boolValue = Preferences.shared[prefKey] as? BoolPreferenceValue {
                let control = Switch()
                control.isOn = boolValue.rawValue
                control.onTintColor = UIColor.tint
                control.addPrimaryAction {
                    Preferences.shared[prefKey] = BoolPreferenceValue(rawValue: control.isOn)
                }
                valueControl = control
            }
            else {
                let button = UIButton.system(prominent: false)
                // we modify the selected title to be != than the corresponding option title, or on macOS the corresponding
                // menu option won't be shown
                button.setTitle(Preferences.shared[prefKey].description + " ", for: .normal)
                button.showsMenuAsPrimaryAction = true
                button.menu = UIMenu(children: Preferences.shared[prefKey].allValues.map { value in
                    let action = UIAction(title: value.description) { action in
                        Preferences.shared[prefKey] = value
                    }
                    action.state = Preferences.shared[prefKey].description == value.description ? .on : .off
                    return action
                })
                valueControl = button
            }
        }
        else {
            valueControl = nil
            accessibilityIdentifier = nil
        }
        
        titleLabel.isUserInteractionEnabled = true

        if valueControl != nil {
            titleTooltip.toolTip = option?.localizedDescr
        } else {
            titleTooltip.toolTip = nil
        }
    }

    // MARK: Layout
    // cell heights are not proper because they depend on layoutMargins, that change according to safeAreaInsets. we only use those for estimated heights
    private static let sizingCell = UINib(nibName: "OptionCell", bundle: nil).instantiate(withOwner: nil, options: nil).first as! OptionCell
    private var isSizingCell: Bool {
        return self == type(of: self).sizingCell
    }
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

        let valueView: UIView = valueControl ?? valueLabel
        valueView.isHidden = false

        let hiddenValueView = valueControl != nil ? valueLabel : nil
        hiddenValueView?.snp.removeConstraints()
        hiddenValueView?.isHidden = true

        if option != nil && useMacOSLayout {
            titleLabel.snp.remakeConstraints { (make) in
                make.top.equalTo(contentView.layoutMarginsGuide)
                make.left.equalTo(contentView.layoutMarginsGuide)
                make.width.equalTo(contentView.layoutMarginsGuide).multipliedBy(0.35)
            }

            valueView.snp.remakeConstraints { (make) in
                make.left.equalTo(titleLabel.snp.right).offset(20)
                make.top.right.equalTo(contentView.layoutMarginsGuide)
                make.height.equalTo(titleLabel).priority(760)
                make.centerY.equalTo(titleLabel)
            }

            descrLabel.snp.remakeConstraints { (make) in
                make.top.equalTo(titleLabel.snp.bottom).offset(showDescription ? 20 : 0).priority(ConstraintPriority.required.value - 1)
                make.top.greaterThanOrEqualTo(titleLabel.snp.bottom).offset(showDescription ? 20 : 0)
                make.top.greaterThanOrEqualTo(valueView.snp.bottom).offset(showDescription ? 20 : 0)
                make.left.right.equalTo(contentView.layoutMarginsGuide)
                make.bottom.equalTo(contentView.layoutMarginsGuide).priority(950)
                if !showDescription {
                    make.height.equalTo(0)
                }
            }
        }
        else if traitCollection.preferredContentSizeCategory.isAccessibilitySize {
            titleLabel.snp.remakeConstraints { (make) in
                make.top.left.right.equalTo(contentView.layoutMarginsGuide)
            }

            valueView.snp.remakeConstraints { (make) in
                make.top.equalTo(titleLabel.snp.bottom).offset(10)
                make.left.right.equalTo(contentView.layoutMarginsGuide)
            }
            
            descrLabel.snp.remakeConstraints { (make) in
                make.top.equalTo(valueView.snp.bottom).offset(showDescription ? 20 : 0)
                make.left.right.equalTo(contentView.layoutMarginsGuide)
                make.bottom.equalTo(contentView.layoutMarginsGuide).priority(950)
                if !showDescription {
                    make.height.equalTo(0)
                }
            }
        }
        else {
            titleLabel.snp.remakeConstraints { (make) in
                make.top.equalTo(contentView.layoutMarginsGuide).priority(980)
                make.top.greaterThanOrEqualTo(contentView.layoutMarginsGuide)
                make.left.equalTo(contentView.layoutMarginsGuide)
            }

            valueView.snp.remakeConstraints { (make) in
                make.left.equalTo(titleLabel.snp.right).offset(20)
                make.top.right.equalTo(contentView.layoutMarginsGuide)
                make.width.lessThanOrEqualToSuperview().multipliedBy(0.5)
                
                if valueControl is Switch {
                    make.centerY.equalTo(titleLabel)
                }
                else {
                    make.firstBaseline.equalTo(titleLabel)
                }
            }

            descrLabel.snp.remakeConstraints { (make) in
                let margin: CGFloat = showDescription ? 12 : 0
                make.top.equalTo(titleLabel.snp.bottom).offset(margin).priority(ConstraintPriority.required.value - 1)
                make.top.greaterThanOrEqualTo(titleLabel.snp.bottom).offset(margin)
                make.top.greaterThanOrEqualTo(valueView.snp.bottom).offset(margin)
                make.left.right.equalTo(contentView.layoutMarginsGuide)
                make.bottom.equalTo(contentView.layoutMarginsGuide).priority(950)
                if !showDescription {
                    make.height.equalTo(0)
                }
            }
        }
        
        super.updateConstraints()
        invalidateIntrinsicContentSize()
    }
}

extension OptionCell: DeviceOptionControllable {
    func updateDeviceOptionControlForDisabledOption() {
        valueControl = nil
    }
    
    func updateDeviceOptionControlForList<T>(option: DeviceOptionTyped<T>, current: T, values: [T], supportsAuto: Bool) where T : CustomStringConvertible, T : Equatable {
        guard !isSizingCell else { return }

        var dropdownOptions: [(String, T?)] = values.map {
            let title = option.stringForValue($0, userFacing: true)
            return (title, $0)
        }
        
        if option.capabilities.contains(.automatic) {
            dropdownOptions.append((L10n.optionValueAuto, nil))
        }

        let optionTitles: [String] = dropdownOptions.map(\.0)
        let initialIndex = values.firstIndex(of: option.value)
        let initialTitle = initialIndex.map { optionTitles[$0] }

        let valueChangedClosure = { [weak self] (selectedIndex: Int) -> Void in
            guard let self = self else { return }
            self.delegate?.deviceOptionControllable(self, willUpdate: option)
            if let selectedValue = dropdownOptions[selectedIndex].1 {
                Sane.shared.updateOption(option, with: .value(selectedValue), completion: self.optionUpdateCompletion(_:))
            } else {
                Sane.shared.updateOption(option, with: .auto, completion: self.optionUpdateCompletion(_:))
            }
        }
        
        let button = UIButton.system(prominent: false)
        // we modify the selected title to be != than the corresponding option title, or on macOS the corresponding
        // menu option won't be shown
        button.setTitle((initialTitle ?? "") + " ", for: .normal)
        button.isEnabled = !option.disabledOrReadOnly
        button.showsMenuAsPrimaryAction = true
        button.menu = UIMenu(title: option.localizedDescr, children: dropdownOptions.enumerated().map { i, value in
            let action = UIAction(title: value.0) { action in
                valueChangedClosure(i)
            }
            action.accessibilityIdentifier = "\(option.identifier)-\(i)"
            action.state = i == initialIndex ? .on : .off
            return action
        })
        valueControl = button
    }
    
    func updateDeviceOptionControlForRange<T>(option: DeviceOptionTyped<T>, current: Double, min: Double, max: Double, step: Double?, supportsAuto: Bool) where T : CustomStringConvertible, T : Numeric {
        guard !isSizingCell else { return }
        guard UIDevice.isCatalyst else { return }

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
        guard UIDevice.isCatalyst else { return }

        #if targetEnvironment(macCatalyst)
        let field = UITextField()
        field.autocorrectionType = .no
        field.autocapitalizationType = .none
        field.text = option.value.description
        field.borderStyle = .roundedRect
        field.isEnabled = !option.disabledOrReadOnly

        // default intrinsic height is a bit too low, let's fix that
        field.translatesAutoresizingMaskIntoConstraints = false
        field.heightAnchor.constraint(greaterThanOrEqualToConstant: 24).isActive = true

        switch kind {
        case .string:   field.keyboardType = .asciiCapable
        case .int:      field.keyboardType = .numberPad
        case .double:   field.keyboardType = .decimalPad
        }
        field.addTarget(self, action: #selector(deviceOptionTextFieldValueChanged), for: .primaryActionTriggered)
        
        // buttons
        // yep, using NSAttributedString to show a UIImage... this is because if the button
        // has no title and just an image, then macOS removes the border and background, and
        // shows the image on a clear background, no matter what I tried... I ain't happy
        // about it either
        let saveButton = UIButton.system(prominent: true)
        let saveIcon = NSAttributedString(
            image: UIImage.icon(.checkmark)?.withTintColor(.label),
            offset: .init(x: 0, y: -2),
            size: CGSize(width: 14, height: 13)
        )
        saveButton.setAttributedTitle(saveIcon, for: .normal)
        saveButton.toolTip = L10n.actionSave
        saveButton.accessibilityLabel = L10n.actionSave
        saveButton.addPrimaryAction { [weak self] in
            self?.deviceOptionTextFieldValueChanged(field)
        }
        
        let autoButton = UIButton.system(prominent: true)
        let autoIcon = NSAttributedString(
            image: UIImage.icon(.auto)?.withTintColor(.label),
            offset: .init(x: 0, y: -3),
            size: CGSize(width: 16, height: 15)
        )
        autoButton.setAttributedTitle(autoIcon, for: .normal)
        saveButton.toolTip = L10n.optionValueAuto
        autoButton.accessibilityLabel = L10n.optionValueAuto
        autoButton.addPrimaryAction { [weak self] in
            guard let self else { return }
            Sane.shared.updateOption(option, with: .auto, completion: self.optionUpdateCompletion(_:))
        }

        let stackView = UIStackView(arrangedSubviews: [saveButton])
        stackView.spacing = 2
        stackView.axis = .horizontal
        stackView.alignment = .fill
        if option.capabilities.contains(.automatic) {
            stackView.addArrangedSubview(autoButton)
        }

        field.rightView = stackView
        field.rightViewMode = .always

        valueControl = field
        #endif
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
            Sane.shared.updateOption(option, with: .value(.parse(from: stringValue) ?? option.value), completion: self.optionUpdateCompletion(_:))
        }
    }
    
    func updateDeviceOptionControlForButton(option: DeviceOptionButton) {
        let button = UIButton.system(prominent: true)
        button.setTitle(L10n.actionPress, for: .normal)
        button.isEnabled = !option.disabledOrReadOnly
        button.addPrimaryAction { [weak self] in
            guard let self = self else { return }
            self.delegate?.deviceOptionControllable(self, willUpdate: option)
            option.press(self.optionUpdateCompletion(_:))
        }
        valueControl = button
    }

    private func optionUpdateCompletion(_ result: Result<SaneInfo, SaneError>) {
        guard let option = option else { return }
        delegate?.deviceOptionControllable(self, didUpdate: option, result: result)
    }
}
