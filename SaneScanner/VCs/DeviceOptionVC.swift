//
//  DeviceOptionVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 03/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SYKit

#if !targetEnvironment(macCatalyst)
import SVProgressHUD
#endif

class DeviceOptionVC : UIAlertController {

    // MARK: Init
    init(option: DeviceOption) {
        self.option = option
        super.init(nibName: nil, bundle: nil)
        
        title = option.localizedTitle
        message = option.localizedDescr
        
        updateContent()
        
        let closeAction = addAction(title: "ACTION CLOSE".localized, style: .cancel, handler: nil)
        self.preferredAction = closeAction
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: Properties
    let option: DeviceOption
    var closeBlock: (() -> ())?
    
    override var preferredStyle: UIAlertController.Style {
        return .actionSheet
    }
    
    // MARK: Completion
    private func optionUpdateCompletion(_ error: Error?) {
        if let error = error {
            SVProgressHUD.showError(withStatus: error.localizedDescription)
        }
        else {
            SVProgressHUD.dismiss()
        }
        
        dismiss(animated: true, completion: nil)
        closeBlock?()
    }
}

// MARK: Content
extension DeviceOptionVC {
    private func updateContent() {
        if option.disabledOrReadOnly || option.hasSingleOption {
            return
        }
        
        if let option = option as? DeviceOptionBool {
            updateForValueList(for: option, values: [true, false])
        }
        else if let option = option as? DeviceOptionInt {
            if case .range(let min, let max) = option.constraint {
                updateForRange(for: option, current: Double(option.value), min: Double(min), max: Double(max), step: nil)
            }
            else if case .stepRange(let min, let max, let step, _) = option.constraint {
                updateForRange(for: option, current: Double(option.value), min: Double(min), max: Double(max), step: Double(step))
            }
            else if case let .list(values) = option.constraint {
                updateForValueList(for: option, values: values)
            }
            else {
                fatalError("Int option should have a constraint!")
            }
        }
        else if let option = option as? DeviceOptionFixed {
            if case .range(let min, let max) = option.constraint {
                updateForRange(for: option, current: option.value, min: Double(min), max: Double(max), step: nil)
            }
            else if case .stepRange(let min, let max, let step, _) = option.constraint {
                updateForRange(for: option, current: option.value, min: Double(min), max: Double(max), step: Double(step))
            }
            else if case let .list(values) = option.constraint {
                updateForValueList(for: option, values: values)
            }
            else {
                fatalError("Int option should have a constraint!")
            }
        }
        else if let option = option as? DeviceOptionString {
            if case let .list(values) = option.constraint {
                updateForValueList(for: option, values: values)
            }
            else {
                updateForTextInput(option)
            }
        }
        else if let option = option as? DeviceOptionButton {
            updateForButton(option)
        }
        else if option is DeviceOptionGroup {
        }
        else {
            fatalError("ALL OPTION TYPES SHOULD ALREADY BE HANDLED")
        }
    }
    
    private func updateForValueList<V, T: DeviceOptionTyped<V>>(for option: T, values: [T.Value]) {
        for value in values {
            addAction(title: option.stringForValue(value, userFacing: true), style: .default) { (_) in
                SVProgressHUD.show()
                Sane.shared.updateOption(option, with: .value(value), completion: self.optionUpdateCompletion(_:))
            }
        }
        
        addAutoButton(for: option)
    }

    private func updateForRange<V, T: DeviceOptionTyped<V>>(for option: T, current: Double, min: Double, max: Double, step: Double?) {
        let optionInt = option as? DeviceOptionInt
        let optionFixed = option as? DeviceOptionFixed
        
        guard optionInt != nil || optionFixed != nil else {
            fatalError("This method should only be used with numeric options")
        }
        
        var stepperVC: SYStepperViewController?
        var sliderVC: SYSliderViewController?
        
        if let step = step {
            let vc = SYStepperViewController(current: current, min: min, max: max, step: step) { (value) -> String? in
                return optionInt?.stringForValue(Int(value), userFacing: true) ?? optionFixed?.stringForValue(Double(value), userFacing: true)
            }
            setContentViewController(vc)
            stepperVC = vc
        }
        else {
            let vc = SYSliderViewController(current: current, min: min, max: max) { (value) -> String? in
                return optionInt?.stringForValue(Int(value), userFacing: true) ?? optionFixed?.stringForValue(Double(value), userFacing: true)
            }
            setContentViewController(vc)
            sliderVC = vc
        }
        
        addAction(title: "ACTION SET VALUE".localized, style: .default) { (_) in
            SVProgressHUD.show()
            let value = stepperVC?.stepperValue ?? sliderVC?.sliderValue ?? 0
            if let optionInt = optionInt {
                Sane.shared.updateOption(optionInt, with: .value(Int(value)), completion: self.optionUpdateCompletion(_:))
            }
            if let optionFixed = optionFixed {
                Sane.shared.updateOption(optionFixed, with: .value(value), completion: self.optionUpdateCompletion(_:))
            }
        }
        
        addAutoButton(for: option)
    }
    
    private func updateForButton(_ option: DeviceOptionButton) {
        addAction(title: "ACTION PRESS".localized, style: .default) { (_) in
            SVProgressHUD.show()
            option.press(self.optionUpdateCompletion(_:))
        }
    }
    
    private func updateForTextInput(_ option: DeviceOptionString) {
        addTextField { (field) in
            field.autocorrectionType = .no
            field.autocapitalizationType = .none
            field.text = option.value
        }
        
        addAction(title: "ACTION SET VALUE".localized, style: .default) { (_) in
            SVProgressHUD.show()
            Sane.shared.updateOption(option, with: .value(self.textFields?.first?.text ?? ""), completion: self.optionUpdateCompletion(_:))
        }
        
        addAutoButton(for: option)
    }
    
    private func addAutoButton<V, T: DeviceOptionTyped<V>>(for option: T) {
        if option.capabilities.contains(.automatic) {
            addAction(title: "OPTION VALUE AUTO".localized, style: .default) { (_) in
                SVProgressHUD.show()
                Sane.shared.updateOption(option, with: .auto, completion: self.optionUpdateCompletion(_:))
            }
        }
    }
}

fileprivate class SYSliderViewController: UIViewController {
    // MARK: Init
    init(current: Double, min: Double, max: Double, conversion: @escaping (Double) -> String?) {
        self.conversion = conversion
        super.init(nibName: nil, bundle: nil)
        label.textColor = .normalText
        label.textAlignment = .center
        label.font = .preferredFont(forTextStyle: .body)
        label.autoAdjustsFontSize = true
        slider.minimumValue = Float(min)
        slider.maximumValue = Float(max)
        slider.value        = Float(current)
        slider.addTarget(self, action: #selector(self.sliderChanged), for: .valueChanged)
        updateText()
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        view.addSubview(label)
        label.setContentHuggingPriority(UILayoutPriority.defaultLow - 1, for: NSLayoutConstraint.Axis.horizontal)
        label.snp.makeConstraints { (make) in
            make.top.greaterThanOrEqualToSuperview()
            make.left.equalTo(20)
            make.right.equalTo(-20)
            make.bottom.equalTo(view.snp.centerY).offset(-10)
        }

        view.addSubview(slider)
        slider.snp.makeConstraints { (make) in
            make.top.equalTo(view.snp.centerY)
            make.left.equalTo(20)
            make.right.equalTo(-20)
            make.bottom.lessThanOrEqualTo(-10)
        }
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    private let conversion: (Double) -> String?
    
    var sliderValue: Double {
        return Double(slider.value)
    }
    
    // MARK: Views
    private let slider = UISlider()
    private let label = UILabel()
    
    // MARK: Actions
    @objc private func sliderChanged() {
        updateText()
    }
    
    // MARK: Content
    private func updateText() {
        label.text = conversion(sliderValue)
    }
}


fileprivate class SYStepperViewController: UIViewController {
    // MARK: Init
    init(current: Double, min: Double, max: Double, step: Double, conversion: @escaping (Double) -> String?) {
        self.conversion = conversion
        super.init(nibName: nil, bundle: nil)
        label.textColor = .normalText
        label.font = .preferredFont(forTextStyle: .body)
        label.autoAdjustsFontSize = true
        stepper.minimumValue = min
        stepper.maximumValue = max
        stepper.stepValue    = step
        stepper.value        = current
        stepper.addTarget(self, action: #selector(self.stepperChanged), for: .valueChanged)
        updateText()
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        view.addSubview(stepper)
        stepper.snp.makeConstraints { (make) in
            make.centerY.equalToSuperview()
            make.top.greaterThanOrEqualTo(10)
            make.right.equalTo(-10)
            make.bottom.equalTo(-10)
        }
        
        label.setContentHuggingPriority(UILayoutPriority.defaultLow - 1, for: NSLayoutConstraint.Axis.horizontal)
        view.addSubview(label)
        label.snp.makeConstraints { (make) in
            make.centerY.equalToSuperview()
            make.left.equalTo(20)
            make.right.equalTo(stepper).offset(-20)
        }
    }
  
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    private let conversion: (Double) -> String?
    
    var stepperValue: Double {
        return stepper.value
    }
    
    // MARK: Views
    private let stepper = UIStepper()
    private let label = UILabel()
    
    // MARK: Actions
    @objc private func stepperChanged() {
        updateText()
    }
    
    // MARK: Content
    private func updateText() {
        label.text = conversion(stepperValue)
    }
}
