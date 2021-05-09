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

class DeviceOptionVC : UIAlertController {

    // MARK: Init
    init(option: DeviceOption, delegate: DeviceOptionControllableDelegate?) {
        self.option = option
        self.delegate = delegate
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
    weak var delegate: DeviceOptionControllableDelegate?

    override var preferredStyle: UIAlertController.Style {
        return .actionSheet
    }
    
    // MARK: Completion
    private func optionUpdateCompletion(_ error: Error?) {
        delegate?.deviceOptionControllable(self, didUpdate: option, error: error)
    }
}

// MARK: Content
extension DeviceOptionVC: DeviceOptionControllable {
    private func updateContent() {
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
        
        addAction(title: "ACTION SET VALUE".localized, style: .default) { (_) in
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
        addAction(title: "ACTION PRESS".localized, style: .default) { (_) in
            self.delegate?.deviceOptionControllable(self, willUpdate: option)
            option.press(self.optionUpdateCompletion(_:))
        }
    }
    
    func updateDeviceOptionControlForField<T>(option: DeviceOptionTyped<T>, current: T, kind: DeviceOptionControllableFieldKind, supportsAuto: Bool) where T : CustomStringConvertible, T : Equatable {
        
        addTextField { (field) in
            field.autocorrectionType = .no
            field.autocapitalizationType = .none
            field.text = option.value.description
            
            switch kind {
            case .string:   field.keyboardType = .asciiCapable
            case .int:      field.keyboardType = .numberPad
            case .double:   field.keyboardType = .decimalPad
            }
        }
        
        addAction(title: "ACTION SET VALUE".localized, style: .default) { (_) in
            self.delegate?.deviceOptionControllable(self, willUpdate: option)
            let stringValue = self.textFields?.first?.text ?? ""

            switch kind {
            case .string(let option):
                Sane.shared.updateOption(option, with: .value(stringValue), completion: self.optionUpdateCompletion(_:))
            case .int(let option):
                Sane.shared.updateOption(option, with: .value(Int(stringValue) ?? option.value), completion: self.optionUpdateCompletion(_:))
            case .double(let option):
                Sane.shared.updateOption(option, with: .value(Double(stringValue) ?? option.value), completion: self.optionUpdateCompletion(_:))
            }
        }
        
        addAutoButton(for: option)
    }
    
    private func addAutoButton<V, T: DeviceOptionTyped<V>>(for option: T) {
        if option.capabilities.contains(.automatic) {
            addAction(title: "OPTION VALUE AUTO".localized, style: .default) { (_) in
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

