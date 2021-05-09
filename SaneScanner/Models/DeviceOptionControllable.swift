//
//  DeviceOptionControllable.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation
import SaneSwift

enum DeviceOptionControllableFieldKind {
    case string(option: DeviceOptionString), int(option: DeviceOptionInt), double(option: DeviceOptionFixed)
}

protocol DeviceOptionControllable {
    var delegate: DeviceOptionControllableDelegate? { get set }

    func updateDeviceOptionControlForDisabledOption()
    func updateDeviceOptionControlForList<T: Equatable & CustomStringConvertible>(option: DeviceOptionTyped<T>, current: T, values: [T], supportsAuto: Bool)
    func updateDeviceOptionControlForRange<T: Numeric>(option: DeviceOptionTyped<T>, current: Double, min: Double, max: Double, step: Double?, supportsAuto: Bool)
    func updateDeviceOptionControlForField<T: Equatable & CustomStringConvertible>(option: DeviceOptionTyped<T>, current: T, kind: DeviceOptionControllableFieldKind, supportsAuto: Bool)
    func updateDeviceOptionControlForButton(option: DeviceOptionButton)
}

protocol DeviceOptionControllableDelegate: NSObjectProtocol {
    func deviceOptionControllable(_ controllable: DeviceOptionControllable, willUpdate option: DeviceOption)
    func deviceOptionControllable(_ controllable: DeviceOptionControllable, didUpdate option: DeviceOption, error: Error?)
}

extension DeviceOption {
    func updateDeviceOptionControl(using controllable: DeviceOptionControllable, filterDisabled: Bool, filterSingleOption: Bool) {
        if disabledOrReadOnly && filterDisabled {
            return controllable.updateDeviceOptionControlForDisabledOption()
        }
        
        if hasSingleOption && filterSingleOption {
            return controllable.updateDeviceOptionControlForDisabledOption()
        }
        
        let auto = capabilities.contains(.automatic)
        
        if let option = self as? DeviceOptionBool {
            controllable.updateDeviceOptionControlForList(option: option, current: option.value, values: [true, false], supportsAuto: auto)
        }
        else if let option = self as? DeviceOptionInt {
            if case .range(let min, let max) = option.constraint {
                controllable.updateDeviceOptionControlForRange(option: option, current: Double(option.value), min: Double(min), max: Double(max), step: nil, supportsAuto: auto)
            }
            else if case .stepRange(let min, let max, let step, _) = option.constraint {
                controllable.updateDeviceOptionControlForRange(option: option, current: Double(option.value), min: Double(min), max: Double(max), step: Double(step), supportsAuto: auto)
            }
            else if case let .list(values) = option.constraint {
                controllable.updateDeviceOptionControlForList(option: option, current: option.value, values: values, supportsAuto: auto)
            }
            else {
                controllable.updateDeviceOptionControlForField(option: option, current: option.value, kind: .int(option: option), supportsAuto: auto)
            }
        }
        else if let option = self as? DeviceOptionFixed {
            if case .range(let min, let max) = option.constraint {
                controllable.updateDeviceOptionControlForRange(option: option, current: option.value, min: min, max: max, step: nil, supportsAuto: auto)
            }
            else if case .stepRange(let min, let max, let step, _) = option.constraint {
                controllable.updateDeviceOptionControlForRange(option: option, current: option.value, min: min, max: max, step: step, supportsAuto: auto)
            }
            else if case let .list(values) = option.constraint {
                controllable.updateDeviceOptionControlForList(option: option, current: option.value, values: values, supportsAuto: auto)
            }
            else {
                controllable.updateDeviceOptionControlForField(option: option, current: option.value, kind: .double(option: option), supportsAuto: auto)
            }
        }
        else if let option = self as? DeviceOptionString {
            if case let .list(values) = option.constraint {
                controllable.updateDeviceOptionControlForList(option: option, current: option.value, values: values, supportsAuto: auto)
            }
            else {
                controllable.updateDeviceOptionControlForField(option: option, current: option.value, kind: .string(option: option), supportsAuto: auto)
            }
        }
        else if let option = self as? DeviceOptionButton {
            controllable.updateDeviceOptionControlForButton(option: option)
        }
        else if self is DeviceOptionGroup {
        }
        else {
            fatalError("ALL OPTION TYPES SHOULD ALREADY BE HANDLED")
        }
    }
}
