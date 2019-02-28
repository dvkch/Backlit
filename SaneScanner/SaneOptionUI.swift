//
//  SaneOptionUI.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SVProgressHUD
import PKYStepper

// TODO: show in a popover instead
// TODO: replace by multiple views conforming to simple protocol, option returns the type of view it needs?
class SaneOptionUI: NSObject {

    static func showDetails(for option: DeviceOption) {
        DLAVAlertView(title: option.localizedTitle, message: option.localizedDescr, delegate: nil, cancel: "ACTION CLOSE".localized, others: []).show()
    }
    
    static func showDetailsAndInput(for option: DeviceOption, _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        guard !option.readOnlyOrSingleOption else {
            showDetails(for: option)
            return
        }
        
        if let option = option as? DeviceOptionBool {
            showOptionsInput(for: option, titles: [option.stringForValue(true, userFacing: true), option.stringForValue(false, userFacing: true)], values: [true, false], completion)
        }
        else if let option = option as? DeviceOptionInt {
            if case .range = option.constraint {
                showSliderInput(for: option, or: nil, completion)
            }
            else if case .stepRange = option.constraint {
                showSliderInput(for: option, or: nil, completion)
            }
            else if case let .list(values) = option.constraint {
                showOptionsInput(for: option, titles: values.map { option.stringForValue($0, userFacing: true) }, values: values, completion)
            }
            else {
                fatalError("Int option should have a constraint!")
            }
        }
        else if let option = option as? DeviceOptionFixed {
            if case .range = option.constraint {
                showSliderInput(for: nil, or: option, completion)
            }
            else if case .stepRange = option.constraint {
                showSliderInput(for: nil, or: option, completion)
            }
            else if case let .list(values) = option.constraint {
                showOptionsInput(for: option, titles: values.map { option.stringForValue($0, userFacing: true) }, values: values, completion)
            }
            else {
                fatalError("Int option should have a constraint!")
            }
        }
        else if let option = option as? DeviceOptionString {
            if case let .list(values) = option.constraint {
                showOptionsInput(for: option, titles: values.map { option.stringForValue($0, userFacing: true) }, values: values, completion)
            }
            else {
                showTextInput(for: option, completion)
            }
        }
        else if let option = option as? DeviceOptionButton {
            showButtonInput(for: option, completion)
        }
        else if let option = option as? DeviceOptionGroup {
            showDetails(for: option)
        }
        else {
            fatalError("ALL OPTION TYPES SHOULD ALREADY BE HANDLED")
        }
    }

    private static func showButtonInput(for option: DeviceOptionButton, _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        DLAVAlertView(
            title: option.localizedTitle,
            message: option.localizedDescr,
            delegate: nil,
            cancel: "ACTION CLOSE".localized,
            others: ["ACTION PRESS".localized]
        ).show { (alert, index) in
            guard index != alert?.cancelButtonIndex else { return }
            option.press { (reloadAll, error) in
                completion?(reloadAll, error)
            }
        }
    }
    
    private static func showTextInput(for option: DeviceOptionString, _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        let alertView = DLAVAlertView(title: option.localizedTitle, message: option.localizedDescr, delegate: nil, cancel: "ACTION CLOSE".localized, others: [])
        
        if option.capabilities.contains(.automatic) {
            alertView.addButton(withTitle: "OPTION VALUE AUTO".localized)
        }
        
        alertView.addButton(withTitle: "ACTION SET VALUE".localized)
        alertView.alertViewStyle = .plainTextInput
        alertView.textField(at: 0)?.borderStyle = .none
        
        alertView.show { (alert, index) in
            guard index != alert?.cancelButtonIndex else { return }
            
            let useAuto = index == alert?.firstOtherButtonIndex && option.capabilities.contains(.automatic)
            SVProgressHUD.show()
            Sane.shared.updateOption(option, with: useAuto ? .auto : .value(alertView.textField(at: 0)?.text ?? ""), completion: completion)
        }
    }
    
    private static func showSliderInput(for optionInt: DeviceOptionInt?, or optionFixed: DeviceOptionFixed?, _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        let alertView = DLAVAlertView(
            title: optionInt?.localizedTitle ?? optionFixed?.localizedTitle,
            message: optionInt?.localizedDescr ?? optionFixed?.localizedDescr,
            delegate: nil, cancel: nil, others: []
        )
        
        let hasAuto = optionInt?.capabilities.contains(.automatic) == true || optionFixed?.capabilities.contains(.automatic) == true
        
        if hasAuto {
            alertView.addButton(withTitle: "OPTION VALUE AUTO".localized)
        }
        alertView.addButton(withTitle: "ACTION SET VALUE".localized)
        alertView.addButton(withTitle: "ACTION CLOSE".localized)

        let updateButtonIndex = alertView.firstOtherButtonIndex + (hasAuto ? 1 : 0)
        
        if let option = optionInt {
            switch option.constraint {
            case .stepRange(let min, let max, let step, _):
                let stepper = PKYStepper(frame: CGRect(x: 0, y: 0, width: 300, height: 40))
                stepper.maximum = Float(min)
                stepper.minimum = Float(max)
                stepper.value = Float(option.value)
                stepper.stepInterval = Float(step)
                stepper.valueChangedCallback = { stepper, value in
                    stepper?.countLabel.text = option.stringForValue(Int(value), userFacing: true)
                }
                stepper.setup()
                alertView.contentView = stepper
            case .range(let min, let max):
                alertView.addSlider(
                    withMin: Float(min),
                    max: Float(max),
                    current: Float(option.value))
                { (alert, value) in
                    let valueString = option.stringForValue(Int(value), userFacing: true)
                    let buttonTitle = String(format: "ACTION SET VALUE TO %@".localized, valueString)
                    alert.setText(buttonTitle, forButtonAt: UInt(updateButtonIndex))
                }
            default:
                fatalError("Unhandled constraint should have been handled earlier")
            }
        }

        if let option = optionFixed {
            switch option.constraint {
            case .stepRange(let min, let max, let step, _):
                let stepper = PKYStepper(frame: CGRect(x: 0, y: 0, width: 300, height: 40))
                stepper.maximum = Float(min)
                stepper.minimum = Float(max)
                stepper.value = Float(option.value)
                stepper.stepInterval = Float(step)
                stepper.valueChangedCallback = { stepper, value in
                    stepper?.countLabel.text = option.stringForValue(Double(value), userFacing: true)
                }
                stepper.setup()
                alertView.contentView = stepper
            case .range(let min, let max):
                alertView.addSlider(
                    withMin: Float(min),
                    max: Float(max),
                    current: Float(option.value))
                { (alert, value) in
                    let valueString = option.stringForValue(Double(value), userFacing: true)
                    let buttonTitle = String(format: "ACTION SET VALUE TO %@".localized, valueString)
                    alert.setText(buttonTitle, forButtonAt: UInt(updateButtonIndex))
                }
            default:
                fatalError("Unhandled constraint should have been handled earlier")
            }
        }
        
        
        alertView.show { (alert, index) in
            guard index != alert?.cancelButtonIndex else { return }
            let useAuto = index == alert!.firstOtherButtonIndex && hasAuto
            SVProgressHUD.show()
            
            var value = Float(0)
            if let slider = alert?.contentView as? UISlider {
                value = slider.value
            }
            if let stepper = alert?.contentView as? PKYStepper {
                value = stepper.value
            }
            
            if let option = optionInt {
                Sane.shared.updateOption(option, with: useAuto ? .auto : .value(Int(value)), completion: completion)
            }
            if let option = optionFixed {
                Sane.shared.updateOption(option, with: useAuto ? .auto : .value(Double(value)), completion: completion)
            }
        }
    }
    
    private static func showOptionsInput<V, T: DeviceOptionTyped<V>>(for option: T, titles: [String], values: [T.Value], _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        var optionsTitles = [String]()
        var optionsValues = [T.Value?]()
        
        if option.capabilities.contains(.automatic) {
            optionsTitles.append("OPTION VALUE AUTO".localized)
            optionsValues.append(nil)
        }
        
        optionsTitles.append(contentsOf: titles)
        optionsValues.append(contentsOf: values)
        
        let alertView = DLAVAlertView(
            title: option.localizedTitle,
            message: option.localizedDescr,
            delegate: nil,
            cancel: "ACTION CLOSE".localized,
            others: optionsTitles
        )
        
        alertView.show { (alert, index) in
            guard index != alert?.cancelButtonIndex else { return }
            
            let value: DeviceOptionNewValue<T.Value>
            if let selectedValue = optionsValues[index - alert!.firstOtherButtonIndex] {
                value = .value(selectedValue)
            } else {
                value = .auto
            }

            SVProgressHUD.show()
            Sane.shared.updateOption(option, with: value, completion: completion)
        }
    }
}
