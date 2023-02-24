//
//  DeviceOption.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

// MARK: Basic option
public class DeviceOption {
    
    // MARK: Init
    init(cOption: SANE_Option_Descriptor, index: Int, device: Device) {
        self.index          = index
        self.device         = device
        self.identifier     = cOption.name?.asString()
        self.localizedTitle = cOption.title?.asString()?.saneTranslation ?? ""
        self.localizedDescr = cOption.desc?.asString()?.saneTranslation  ?? ""
        self.capabilities   = SaneCapabilities(rawValue: cOption.cap)
        self.type           = cOption.type
        self.unit           = cOption.unit
        self.size           = Int(cOption.size)
        self.cOption        = cOption
    }
    
    // MARK: Properties
    public let index: Int
    public let device: Device
    public let identifier: String?
    public var localizedTitle: String
    public var localizedDescr: String
    public var capabilities: SaneCapabilities
    internal var type: SANE_Value_Type
    internal var unit: SANE_Unit
    internal var size: Int
    internal var cOption: SANE_Option_Descriptor
    
    // MARK: Computed properties
    public var disabledOrReadOnly: Bool {
        return capabilities.contains(.inactive) || !capabilities.contains(.softwareSettable)
    }
    public var hasSingleOption: Bool {
        return true
    }
    
    // MARK: Value
    public var localizedValue: String {
        return ""
    }
    
    internal func refreshValue(_ block: ((Error?) -> ())?) {
        block?(nil)
    }
    
    // MARK: Helpers
    fileprivate static func assertType(cOption: SANE_Option_Descriptor, type: SANE_Value_Type, class: AnyClass) -> () {
        guard cOption.type == type else {
            let className = String(describing: `class`)
            fatalError("\(className) should only be used with \(type.description) option type")
        }
    }
    
    internal static func typedOption(cOption: SANE_Option_Descriptor, index: Int, device: Device) -> DeviceOption {
        switch cOption.type {
        case SANE_TYPE_BOOL:    return DeviceOptionBool(cOption: cOption, index: index, device: device, initialValue: false)
        case SANE_TYPE_INT:     return DeviceOptionInt(cOption: cOption, index: index, device: device, initialValue: 0)
        case SANE_TYPE_FIXED:   return DeviceOptionFixed(cOption: cOption, index: index, device: device, initialValue: 0)
        case SANE_TYPE_STRING:  return DeviceOptionString(cOption: cOption, index: index, device: device, initialValue: "")
        case SANE_TYPE_BUTTON:  return DeviceOptionButton(cOption: cOption, index: index, device: device, initialValue: false)
        case SANE_TYPE_GROUP:   return DeviceOptionGroup(cOption: cOption, index: index, device: device)
        default: fatalError("Unsupported type \(cOption.type) for option at index \(index) in device \(device.description)")
        }
    }
}

extension DeviceOption: CustomStringConvertible {
    public var description: String {
        return "\(Swift.type(of: self)): \(index), \(localizedTitle), \(type.description), \(unit.description)"
    }
}

extension DeviceOption: Equatable {
    public static func == (lhs: DeviceOption, rhs: DeviceOption) -> Bool {
        return lhs.device == rhs.device && lhs.index == rhs.index
    }
}

// MARK: Typed option
public class DeviceOptionTyped<T: Equatable & CustomStringConvertible>: DeviceOption {
    
    // MARK: Init
    init(cOption: SANE_Option_Descriptor, index: Int, device: Device, initialValue: T) {
        self.value = initialValue
        super.init(cOption: cOption, index: index, device: device)
    }
    
    // MARK: Value
    public internal(set) var value: T
    
    public func stringForValue(_ value: T, userFacing: Bool) -> String {
        fatalError("Not implemented")
    }
    
    internal func bytesForValue(_ value: T) -> Data {
        fatalError("Not implemented")
    }
    
    internal func valueForBytes(_ bytes: UnsafeRawPointer) -> T {
        fatalError("Not implemented")
    }
    
    public var constraint: OptionConstraint<T> {
        fatalError("Not implemented")
    }
    
    public var bestPreviewValue: DeviceOptionNewValue<T> {
        fatalError("Not implemented")
    }
    
    internal override func refreshValue(_ block: ((Error?) -> ())?) {
        Sane.shared.valueForOption(self) { (value, error) in
            if let value = value, error == nil {
                self.value = value
            }
            block?(error)
        }
    }
    
    public override var hasSingleOption: Bool {
        return constraint.hasSingleValue
    }
}

// MARK: Value option protocol
public protocol DeviceOptionTypedProtocol: AnyObject {
    associatedtype Value: Equatable & CustomStringConvertible
    var value: Value { get }
}

extension DeviceOptionTyped : DeviceOptionTypedProtocol { }

// MARK: Typed option value
public enum DeviceOptionNewValue<T> {
    case none, auto, value(T)
}

// MARK: Option constraints
public enum OptionConstraint<T: Equatable & CustomStringConvertible> {
    case none, range(min: T, max: T), stepRange(min: T, max: T, step: T, values: [T]), list([T])
    
    var hasSingleValue: Bool {
        switch self {
        case .none: return false
        case .range(let min, let max): return min == max
        case .stepRange(_, _, _, let values), .list(let values): return values.count == 1
        }
    }
}

extension OptionConstraint : CustomStringConvertible {
    public var description: String {
        switch self {
        case .none:
            return "OPTION CONSTRAINED NOT CONSTRAINED".saneTranslation
            
        case .range(let min, let max):
            return String(format: "OPTION CONSTRAINED RANGE FROM TO %@ %@".saneTranslation, min.description, max.description)
            
        case .stepRange(let min, let max, let step, _):
            return String(format: "OPTION CONSTRAINED RANGE FROM TO STEP %@ %@ %@".saneTranslation, min.description, max.description, step.description)
            
        case .list(let values):
            let strings = values.map { $0.description }
            return String(format: "OPTION CONSTRAINED LIST %@".saneTranslation, strings.joined(separator: ", "))
        }
    }
}

// MARK: Bool
public class DeviceOptionBool: DeviceOptionTyped<Bool> {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device, initialValue: Bool) {
        DeviceOption.assertType(cOption: cOption, type: SANE_TYPE_BOOL, class: DeviceOptionBool.self)
        super.init(cOption: cOption, index: index, device: device, initialValue: initialValue)
    }
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
    
    public override func stringForValue(_ value: Bool, userFacing: Bool) -> String {
        if userFacing {
            return value ? "OPTION BOOL ON".saneTranslation : "OPTION BOOL OFF".saneTranslation
        }
        return value ? "On" : "Off"
    }
    
    internal override func bytesForValue(_ value: Bool) -> Data {
        var data = Data(repeating: 0, count: size)
        data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) -> () in
            bytes.bindMemory(to: SANE_Bool.self).baseAddress?.pointee = value ? SANE_TRUE : SANE_FALSE
        }
        return data
    }
    
    internal override func valueForBytes(_ bytes: UnsafeRawPointer) -> Bool {
        return bytes.bindMemory(to: SANE_Bool.self, capacity: size).pointee == SANE_TRUE
    }

    public override var constraint: OptionConstraint<Bool> {
        return .none
    }
    
    public override var bestPreviewValue: DeviceOptionNewValue<Bool> {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return .none }
        switch value {
        case .auto: return capabilities.contains(.automatic) ? .auto : .value(self.value)
        case .on:   return .value(true)
        case .off:  return .value(false)
        default:    fatalError("Unsupported preview value \(value) for option \(identifier ?? "<nil>")")
        }
    }
}

// MARK: Int
public class DeviceOptionInt: DeviceOptionTyped<Int> {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device, initialValue: Int) {
        DeviceOption.assertType(cOption: cOption, type: SANE_TYPE_INT, class: DeviceOptionInt.self)
        super.init(cOption: cOption, index: index, device: device, initialValue: initialValue)
    }
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
    
    public override func stringForValue(_ value: Int, userFacing: Bool) -> String {
        let unitString = userFacing && unit != SANE_UNIT_NONE ? " " + unit.description : ""
        return String(value) + unitString
    }
    
    internal override func bytesForValue(_ value: Int) -> Data {
        var data = Data(repeating: 0, count: size)
        data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) -> () in
            bytes.bindMemory(to: SANE_Int.self).baseAddress?.pointee = SANE_Int(value)
        }
        return data
    }
    
    internal override func valueForBytes(_ bytes: UnsafeRawPointer) -> Int {
        let saneValue = bytes.bindMemory(to: SANE_Int.self, capacity: size).pointee
        return Int(saneValue)
    }

    public override var constraint: OptionConstraint<Int> {
        switch cOption.constraint_type {
        case SANE_CONSTRAINT_NONE:
            return .none
        case SANE_CONSTRAINT_RANGE:
            let minValue = Int(cOption.constraint.range.pointee.min)
            let maxValue = Int(cOption.constraint.range.pointee.max)
            let stepValue = Int(cOption.constraint.range.pointee.quant)
            if stepValue == 0 {
                return OptionConstraint<Int>.range(min: minValue, max: maxValue)
            }
            let values = stride(from: minValue, through: maxValue, by: stepValue)
            return OptionConstraint<Int>.stepRange(min: minValue, max: maxValue, step: stepValue, values: Array(values))
        case SANE_CONSTRAINT_WORD_LIST:
            let count = cOption.constraint.word_list.pointee
            let values = (0..<Int(count))
                .map { cOption.constraint.word_list.advanced(by: $0 + 1).pointee }
                .map { Int($0) }
            return OptionConstraint<Int>.list(values)
        default:
            fatalError("Unsupported constraint type for DeviceOptionInt")
        }
    }
    
    public override var bestPreviewValue: DeviceOptionNewValue<Int> {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return .none }
        switch value {
        case .auto:
            return capabilities.contains(.automatic) ? .auto : .value(self.value)

        case .min, .max:
            switch constraint {
            case .range(let min, let max), .stepRange(let min, let max, _, _):
                return value == .min ? .value(min) : .value(max)
            case .list(let values):
                guard !values.isEmpty else { return .none }
                return value == .min ? .value(values.min()!) : .value(values.max()!)
            case .none:
                return .none
            }

        case .value(let specificValue):
            switch constraint {
            case .range(let min, let max), .stepRange(let min, let max, _, _):
                return .value(specificValue.clamped(min: min, max: max))
            case .list(let values):
                guard let closest = values.closest(to: specificValue) else { return .none }
                return .value(closest)
            case .none:
                return .value(specificValue)
            }

        default:
            fatalError("Unsupported preview value \(value) for option \(identifier ?? "<nil>")")
        }
    }
}

// MARK: Fixed float
public class DeviceOptionFixed: DeviceOptionTyped<Double> {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device, initialValue: Double) {
        DeviceOption.assertType(cOption: cOption, type: SANE_TYPE_FIXED, class: DeviceOptionFixed.self)
        super.init(cOption: cOption, index: index, device: device, initialValue: initialValue)
    }
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
    
    public override func stringForValue(_ value: Double, userFacing: Bool) -> String {
        let unitString = userFacing && unit != SANE_UNIT_NONE ? " " + unit.description : ""
        return String(format: "%0.2lf", value) + unitString
    }
    
    internal override func bytesForValue(_ value: Double) -> Data {
        var data = Data(repeating: 0, count: size)
        data.withUnsafeMutableBytes { (bytes: UnsafeMutableRawBufferPointer) -> () in
            bytes.bindMemory(to: SANE_Word.self).baseAddress?.pointee = SaneFixedFromDouble(value)
        }
        return data
    }
    
    internal override func valueForBytes(_ bytes: UnsafeRawPointer) -> Double {
        let saneValue = bytes.bindMemory(to: SANE_Fixed.self, capacity: size).pointee
        return SaneDoubleFromFixed(saneValue)
    }

    public override var constraint: OptionConstraint<Double> {
        switch cOption.constraint_type {
        case SANE_CONSTRAINT_NONE:
            return .none
        case SANE_CONSTRAINT_RANGE:
            let minValue = SaneDoubleFromFixed(cOption.constraint.range.pointee.min)
            let maxValue = SaneDoubleFromFixed(cOption.constraint.range.pointee.max)
            let stepValue = SaneDoubleFromFixed(cOption.constraint.range.pointee.quant)
            if stepValue == 0 {
                return OptionConstraint<Double>.range(min: minValue, max: maxValue)
            }
            let values = stride(from: minValue, through: maxValue, by: stepValue)
            return OptionConstraint<Double>.stepRange(min: minValue, max: maxValue, step: stepValue, values: Array(values))
        case SANE_CONSTRAINT_WORD_LIST:
            let count = cOption.constraint.word_list.pointee
            let values = (0..<Int(count))
                .map { cOption.constraint.word_list.advanced(by: $0 + 1).pointee }
                .map { SaneDoubleFromFixed($0)}
            return OptionConstraint<Double>.list(values)
        default:
            fatalError("Unsupported constraint type for DeviceOptionFixed")
        }
    }
    
    public override var bestPreviewValue: DeviceOptionNewValue<Double> {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return .none }
        switch value {
        case .auto:
            return capabilities.contains(.automatic) ? .auto : .value(self.value)

        case .min, .max:
            switch constraint {
            case .range(let min, let max), .stepRange(let min, let max, _, _):
                return value == .min ? .value(min) : .value(max)
            case .list(let values):
                guard !values.isEmpty else { return .none }
                return value == .min ? .value(values.min()!) : .value(values.max()!)
            case .none:
                return .none
            }
            
        case .value(let specificValue):
            switch constraint {
            case .range(let min, let max), .stepRange(let min, let max, _, _):
                return .value(Double(specificValue).clamped(min: min, max: max))
            case .list(let values):
                guard let closest = values.closest(to: Double(specificValue)) else { return .none }
                return .value(closest)
            case .none:
                return .value(Double(specificValue))
            }

        default:
            fatalError("Unsupported preview value \(value) for option \(identifier ?? "<nil>")")
        }
    }
}

// MARK: Numeric
internal protocol DeviceOptionNumeric {
    var doubleValue: Double { get }
    var bestPreviewDoubleValue: DeviceOptionNewValue<Double> { get }
}

extension DeviceOptionInt : DeviceOptionNumeric {
    var doubleValue: Double {
        return Double(value)
    }
    var bestPreviewDoubleValue: DeviceOptionNewValue<Double> {
        switch self.bestPreviewValue {
        case .value(let value): return .value(Double(value))
        case .auto: return .auto
        case .none: return .none
        }
    }
}

extension DeviceOptionFixed : DeviceOptionNumeric {
    var doubleValue: Double {
        return value
    }
    var bestPreviewDoubleValue: DeviceOptionNewValue<Double> {
        return bestPreviewValue
    }
}

// MARK: String
public class DeviceOptionString: DeviceOptionTyped<String> {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device, initialValue: String) {
        DeviceOption.assertType(cOption: cOption, type: SANE_TYPE_STRING, class: DeviceOptionString.self)
        super.init(cOption: cOption, index: index, device: device, initialValue: initialValue)
    }
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
    
    public override func stringForValue(_ value: String, userFacing: Bool) -> String {
        let unitString = userFacing && unit != SANE_UNIT_NONE ? " " + unit.description : ""
        let stringValue = userFacing ? value.saneTranslation : value
        return stringValue + unitString
    }
    
    internal override func bytesForValue(_ value: String) -> Data {
        return value.data(using: .utf8)?.subarray(maxCount: size) ?? Data()
    }
    
    internal override func valueForBytes(_ bytes: UnsafeRawPointer) -> String {
        let saneValue = bytes.bindMemory(to: SANE_Char.self, capacity: size)
        return String(cString: saneValue)
    }

    public override var constraint: OptionConstraint<String> {
        switch cOption.constraint_type {
        case SANE_CONSTRAINT_NONE:
            return .none
        case SANE_CONSTRAINT_STRING_LIST:
            var values = [String]()
            while let value = cOption.constraint.string_list.advanced(by: values.count).pointee {
                values.append(value.asString() ?? "")
            }
            return OptionConstraint<String>.list(values)
        default:
            fatalError("Unsupported constraint type for DeviceOptionString")
        }
    }

    public override var bestPreviewValue: DeviceOptionNewValue<String> {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return .none }
        if value == .auto {
            return capabilities.contains(.automatic) ? .auto : .value(self.value)
        }
        fatalError("Unsupported preview value \(value) for option \(identifier ?? "<nil>")")
    }
}

// MARK: Button
public class DeviceOptionButton: DeviceOptionTyped<Bool> {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device, initialValue: Bool) {
        DeviceOption.assertType(cOption: cOption, type: SANE_TYPE_BUTTON, class: DeviceOptionButton.self)
        super.init(cOption: cOption, index: index, device: device, initialValue: initialValue)
    }
    
    public func press(_ completion: ((_ reloadAll: Result<SaneInfo, Error>) -> Void)?) {
        Sane.shared.updateOption(self, with: .value(true), completion: completion)
    }

    public override var value: Bool {
        get { return false }
        set { super.value = newValue }
    }
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
    
    public override func stringForValue(_ value: Bool, userFacing: Bool) -> String {
        return ""
    }
    
    internal override func bytesForValue(_ value: Bool) -> Data {
        return Data(repeating: value ? 1 : 0, count: size)
    }
    
    internal override func valueForBytes(_ bytes: UnsafeRawPointer) -> Bool {
        return false
    }

    public override var constraint: OptionConstraint<Bool> {
        return .none
    }
    
    public override var bestPreviewValue: DeviceOptionNewValue<Bool> {
        return .none
    }
}

// MARK: Group
public class DeviceOptionGroup: DeviceOption {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device) {
        DeviceOption.assertType(cOption: cOption, type: SANE_TYPE_GROUP, class: DeviceOptionGroup.self)
        super.init(cOption: cOption, index: index, device: device)
    }
    
    public func options(includeAdvanced: Bool) -> [DeviceOption] {
        let nextGroupIndex = device.options
            .compactMap { ($0 as? DeviceOptionGroup)?.index }
            .sorted()
            .first(where: { $0 > index }) ?? device.options.count
        
        let allOptions = device.options.filter { $0.index > index && $0.index < nextGroupIndex }
        
        var filteredOptions = allOptions
            .filter { $0.identifier != SaneStandardOption.preview.saneIdentifier }
        
        if device.canCrop {
            let cropOptionsIDs = SaneStandardOption.cropOptions.map { $0.saneIdentifier }
            filteredOptions.removeAll(where: { $0.identifier != nil && cropOptionsIDs.contains($0.identifier!) })
        }
        
        if !includeAdvanced {
            filteredOptions.removeAll(where: { $0.capabilities.contains(.advanced) })
        }
        
        return filteredOptions
    }
}
