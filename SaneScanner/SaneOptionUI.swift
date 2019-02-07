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

    static func showDetails(for option: SYSaneOption) {
        DLAVAlertView(title: option.localizedDesc, message: option.localizedDesc, delegate: nil, cancel: "ACTION CLOSE".localized, others: []).show()
    }
    
    static func showDetailsAndInput(for option: SYSaneOption, _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        guard !option.readOnlyOrSingleOption() else {
            showDetails(for: option)
            return
        }
        
        switch option.type {
        case SANE_TYPE_BOOL:
            showOptionsInput(for: option, titles: ["OPTION BOOL ON".localized, "OPTION BOOL OFF".localized], values: [true, false], completion)
            
        case SANE_TYPE_INT, SANE_TYPE_FIXED:
            let castOption = option as! SYSaneOptionNumber
            if option.constraintType == SANE_CONSTRAINT_RANGE {
                showSliderInput(for: castOption, completion)
            }
            else {
                showOptionsInput(for: castOption, titles: castOption.constraintValues(withUnit: true), values: castOption.constraintValues, completion)
            }
        case SANE_TYPE_STRING:
            let castOption = option as! SYSaneOptionString
            if castOption.constraintType == SANE_CONSTRAINT_NONE {
                showTextInput(for: castOption, completion)
            }
            else {
                showOptionsInput(for: option, titles: castOption.constraintValues, values: castOption.constraintValues, completion)
            }
        case SANE_TYPE_BUTTON:
            self.showButtonInput(for: option as! SYSaneOptionButton, completion)
            
        case SANE_TYPE_GROUP:
            self.showDetails(for: option)
            
        default:
            fatalError("ALL OPTION TYPES SHOULD ALREADY BE HANDLED")
        }
    }

    private static func showButtonInput(for option: SYSaneOptionButton, _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        DLAVAlertView(
            title: option.localizedTitle,
            message: option.localizedDesc,
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
    
    private static func showTextInput(for option: SYSaneOptionString, _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        let alertView = DLAVAlertView(title: option.localizedTitle, message: option.localizedDesc, delegate: nil, cancel: "ACTION CLOSE".localized, others: [])
        
        if option.capSetAuto {
            alertView.addButton(withTitle: "OPTION VALUE AUTO".localized)
        }
        
        alertView.addButton(withTitle: "ACTION SET VALUE".localized)
        alertView.alertViewStyle = .plainTextInput
        alertView.textField(at: 0)?.borderStyle = .none
        
        alertView.show { (alert, index) in
            guard index != alert?.cancelButtonIndex else { return }
            
            let useAuto = index == alert?.firstOtherButtonIndex && option.capSetAuto
            SVProgressHUD.show()
            Sane.shared.setValueForOption(value: alertView.textField(at: 0)?.text, auto: useAuto, option: option, completion: completion)
        }
    }
    
    private static func showSliderInput(for option: SYSaneOptionNumber, _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        let alertView = DLAVAlertView(title: option.localizedTitle, message: option.localizedDesc, delegate: nil, cancel: nil, others: [])
        
        if option.capSetAuto {
            alertView.addButton(withTitle: "OPTION VALUE AUTO".localized)
        }
        alertView.addButton(withTitle: "ACTION SET VALUE".localized)
        alertView.addButton(withTitle: "ACTION CLOSE".localized)

        let updateButtonIndex = alertView.firstOtherButtonIndex + (option.capSetAuto ? 1 : 0)
        
        if option.type == SANE_TYPE_INT || option.type == SANE_TYPE_FIXED {
            if let stepValue = option.stepValue {
                let stepper = PKYStepper(frame: CGRect(x: 0, y: 0, width: 300, height: 40))
                stepper.maximum = option.maxValue.floatValue
                stepper.minimum = option.minValue.floatValue
                stepper.value = option.value.floatValue
                stepper.stepInterval = stepValue.floatValue
                stepper.valueChangedCallback = { stepper, value in
                    stepper?.countLabel.text = option.string(forValue: value, withUnit: true)
                }
                stepper.setup()
                alertView.contentView = stepper
            }
            else {
                alertView.addSlider(withMin: option.minValue.floatValue, max: option.maxValue.floatValue, current: option.value.floatValue) { (alert, value) in
                    let valueString = option.string(forValue: value, withUnit: true)!
                    let buttonTitle = String(format: "ACTION SET VALUE TO %@".localized, valueString)
                    alert?.setText(buttonTitle, forButtonAt: UInt(updateButtonIndex))
                }
            }
        }
        
        alertView.show { (alert, index) in
            guard index != alert?.cancelButtonIndex else { return }
            let useAuto = index == alert!.firstOtherButtonIndex && option.capSetAuto
            SVProgressHUD.show()
            
            var value: Any?
            if let slider = alert?.contentView as? UISlider {
                value = slider.value
            }
            if let stepper = alert?.contentView as? PKYStepper {
                value = stepper.value
            }
            
            if let vFloat = value as? Float, option.type == SANE_TYPE_INT {
                value = Int(vFloat)
            }
            
            Sane.shared.setValueForOption(value: value, auto: useAuto, option: option, completion: completion)
        }
    }
    
    private static func showOptionsInput(for option: SYSaneOption, titles: [String], values: [Any], _ completion: ((_ reloadAll: Bool, _ error: Error?) -> Void)?) {
        var optionsTitles = [String]()
        var optionsValues = [Any]()
        
        if option.capSetAuto {
            optionsTitles.append("OPTION VALUE AUTO".localized)
            optionsValues.append(NSNull())
        }
        
        optionsTitles.append(contentsOf: titles)
        optionsValues.append(contentsOf: values)
        
        let alertView = DLAVAlertView(
            title: option.localizedTitle,
            message: option.localizedDesc,
            delegate: nil,
            cancel: "ACTION CLOSE".localized,
            others: optionsTitles
        )
        
        alertView.show { (alert, index) in
            guard index != alert?.cancelButtonIndex else { return }
            
            let value = optionsValues[index - alert!.firstOtherButtonIndex]
            let useAuto = (value is NSNull)

            SVProgressHUD.show()
            Sane.shared.setValueForOption(value: value, auto: useAuto, option: option, completion: completion)
        }
    }
}