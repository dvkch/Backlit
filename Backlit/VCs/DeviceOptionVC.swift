//
//  DeviceOptionVC.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 03/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SYKit

class DeviceOptionVC : UIAlertController {

    // MARK: Init
    init(option: DeviceOption, showHelpOnly: Bool, delegate: DeviceOptionControllableDelegate?) {
        self.option = option
        self.showHelpOnly = showHelpOnly
        self.delegate = delegate
        super.init(nibName: nil, bundle: nil)
        
        self.view.tintColor = .normalText
        
        title = option.localizedTitle
        message = option.localizedDescr
        
        updateContent()
        
        let closeAction = addAction(title: L10n.actionClose, style: .cancel, handler: nil)
        self.preferredAction = closeAction
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: Properties
    private let option: DeviceOption
    private let showHelpOnly: Bool
    weak var delegate: DeviceOptionControllableDelegate?

    override var preferredStyle: UIAlertController.Style {
        return .actionSheet
    }
    
    // MARK: Completion
    private func optionUpdateCompletion(_ result: Result<SaneInfo, SaneError>) {
        delegate?.deviceOptionControllable(self, didUpdate: option, result: result)
    }
}

// MARK: Content
extension DeviceOptionVC: DeviceOptionControllable {
    private func updateContent() {
        guard !showHelpOnly else { return }
        option.updateDeviceOptionControl(using: self, filterDisabled: true, filterSingleOption: true)
    }
    
    func updateDeviceOptionControlForDisabledOption() {
    }
    
    func updateDeviceOptionControlForList<T>(option: DeviceOptionTyped<T>, current: T, values: [T], supportsAuto: Bool) where T : CustomStringConvertible, T : Equatable {
        for value in values {
            addAction(title: option.stringForValue(value, userFacing: true), style: .default) { (_) in
                self.delegate?.deviceOptionControllable(self, willUpdate: option)
                Sane.shared.updateOption(option, with: .value(value), completion: self.optionUpdateCompletion(_:))
            }
        }
        
        addAutoButton(for: option)
    }
    
    func updateDeviceOptionControlForRange<T>(option: DeviceOptionTyped<T>, current: Double, min: Double, max: Double, step: Double?, supportsAuto: Bool) where T : Numeric {
        let optionInt = option as? DeviceOptionInt
        let optionFixed = option as? DeviceOptionFixed
        
        guard optionInt != nil || optionFixed != nil else {
            fatalError("This method should only be used with numeric options")
        }
        
        let sliderVC: SYSliderViewController = SYSliderViewController(current: current, min: min, max: max, step: step) { (value) -> String in
            return optionInt?.stringForValue(Int(value), userFacing: true) ?? optionFixed?.stringForValue(value, userFacing: true) ?? String(value)
        }
        setContentViewController(sliderVC)
        
        addAction(title: L10n.actionSetValue, style: .default) { (_) in
            self.delegate?.deviceOptionControllable(self, willUpdate: option)
            if let optionInt = optionInt {
                Sane.shared.updateOption(optionInt, with: .value(Int(sliderVC.sliderValue)), completion: self.optionUpdateCompletion(_:))
            }
            if let optionFixed = optionFixed {
                Sane.shared.updateOption(optionFixed, with: .value(sliderVC.sliderValue), completion: self.optionUpdateCompletion(_:))
            }
        }
        
        addAutoButton(for: option)
    }
    
    func updateDeviceOptionControlForButton(option: DeviceOptionButton) {
        addAction(title: L10n.actionPress, style: .default) { (_) in
            self.delegate?.deviceOptionControllable(self, willUpdate: option)
            option.press(self.optionUpdateCompletion(_:))
        }
    }
    
    func updateDeviceOptionControlForField<T>(option: DeviceOptionTyped<T>, current: T, kind: DeviceOptionControllableFieldKind, supportsAuto: Bool) where T : CustomStringConvertible, T : Equatable {
        
        let textFieldVC: SYTextInputViewController = .init(current: current.description, kind: kind)
        setContentViewController(textFieldVC)

        addAction(title: L10n.actionSetValue, style: .default) { (_) in
            self.delegate?.deviceOptionControllable(self, willUpdate: option)

            switch kind {
            case .string(let option):
                Sane.shared.updateOption(option, with: .value(textFieldVC.stringValue), completion: self.optionUpdateCompletion(_:))
            case .int(let option):
                Sane.shared.updateOption(option, with: .value(Int(textFieldVC.stringValue) ?? option.value), completion: self.optionUpdateCompletion(_:))
            case .double(let option):
                Sane.shared.updateOption(option, with: .value(.parse(from: textFieldVC.stringValue) ?? option.value), completion: self.optionUpdateCompletion(_:))
            }
        }
        
        addAutoButton(for: option)
    }
    
    private func addAutoButton<V, T: DeviceOptionTyped<V>>(for option: T) {
        if option.capabilities.contains(.automatic) {
            addAction(title: L10n.optionValueAuto, style: .default) { (_) in
                self.delegate?.deviceOptionControllable(self, willUpdate: option)
                Sane.shared.updateOption(option, with: .auto, completion: self.optionUpdateCompletion(_:))
            }
        }
    }
}

fileprivate class SYSliderViewController: UIViewController {
    // MARK: Init
    init(current: Double, min: Double, max: Double, step: Double?, conversion: @escaping (Double) -> String  ) {
        super.init(nibName: nil, bundle: nil)
        slider.minimumValue = Float(min)
        slider.maximumValue = Float(max)
        slider.value = Float(current)
        slider.step = step.flatMap { Float($0) }
        slider.formatter = { conversion(Double($0)) }
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        view.addSubview(slider)
        slider.snp.makeConstraints { (make) in
            make.top.equalTo(10)
            make.left.equalTo(20)
            make.right.equalTo(-20)
            make.bottom.equalTo(-10)
        }
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    var sliderValue: Double {
        return Double(slider.value)
    }
    
    // MARK: Views
    private let slider = Slider()
}

fileprivate class SYTextInputViewController: UIViewController {
    // MARK: Init
    init(current: String, kind: DeviceOptionControllableFieldKind) {
        super.init(nibName: nil, bundle: nil)

        textField.text = current
        textField.font = .preferredFont(forTextStyle: .body)
        textField.backgroundColor = .background
        textField.layer.cornerRadius = 5
        textField.autocorrectionType = .no
        textField.autocapitalizationType = .sentences

        switch kind {
        case .string:   textField.keyboardType = .asciiCapable
        case .int:      textField.keyboardType = .numberPad
        case .double:   textField.keyboardType = .decimalPad
        }
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        view.addSubview(textField)
        textField.delegate = self
        textField.snp.makeConstraints { (make) in
            make.top.equalTo(10)
            make.left.equalTo(20)
            make.right.equalTo(-20)
            make.bottom.equalTo(-10)
        }
        textFieldHeight = textField.heightAnchor.constraint(greaterThanOrEqualToConstant: 0)
        textFieldHeight.priority = .defaultHigh
        textFieldHeight.isActive = true
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    var stringValue: String {
        return textField.text ?? ""
    }
    
    // MARK: Views
    private let textField = UITextView()
    private var textFieldHeight: NSLayoutConstraint!
    
    // MARK: Layout
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        textField.layoutIfNeeded()
        textFieldHeight.constant = textField.contentSize.height
    }
}

extension SYTextInputViewController: UITextViewDelegate {
    func textViewDidChange(_ textView: UITextView) {
        view.setNeedsLayout()
    }
}
