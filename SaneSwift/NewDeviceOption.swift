//
//  DeviceOption.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//


// MARK: Basic option
public class SaneOption {
    
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
    public var readOnlyOrSingleOption: Bool {
        return capabilities.contains(.inactive) || !capabilities.contains(.softwareSettable)
    }
    
    // MARK: Value
    internal func refreshValue(_ block: ((Error?) -> ())?) {
        // TODO: implement
        //fatalError("Not implemented")
    }
    
    public var localizedValue: String {
        return ""
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
        case SANE_TYPE_BOOL:    return DeviceOptionBool(cOption: cOption, index: index, device: device)
        case SANE_TYPE_INT:     return DeviceOptionInt(cOption: cOption, index: index, device: device)
        case SANE_TYPE_FIXED:   return DeviceOptionFixed(cOption: cOption, index: index, device: device)
        case SANE_TYPE_STRING:  return DeviceOptionString(cOption: cOption, index: index, device: device)
        case SANE_TYPE_BUTTON:  return DeviceOptionButton(cOption: cOption, index: index, device: device)
        case SANE_TYPE_GROUP:   return DeviceOptionGroup(cOption: cOption, index: index, device: device)
        default: fatalError("Unsupported type \(cOption.type) for option at index \(index) in device \(device.description)")
        }
    }
}

extension SaneOption: CustomStringConvertible {
    public var description: String {
        return "\(Swift.type(of: self)): \(index), \(localizedTitle), \(type.description), \(unit.description)"
    }
}


// MARK: Option constraints
public enum OptionConstraint<T: Equatable & CustomStringConvertible> {
    case none, range(min: T, max: T), stepRange(min: T, max: T, step: T, values: [T]), list([T])
    
    var singleValue: Bool {
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

// MARK: Value option protocol
public protocol DeviceOptionTyped {
    associatedtype Value: Equatable & CustomStringConvertible
    
    var value: Value { get }
    
    func stringForValue(_ value: Value, userFacing: Bool) -> String
    func bytesForValue(_ value: Value) -> Data

    var constraint: OptionConstraint<Value> { get }
}

internal protocol DeviceOptionTypedInternal : DeviceOptionTyped {
    mutating func updateFromBytes(_ bytes: UnsafeRawPointer)
    
    var bestPreviewValue: DeviceOptionNewValue<Value> { get }
}

public enum DeviceOptionNewValue<T> {
    case none, auto, value(T)
}

// MARK: Bool
public class SaneOptionBool: SaneOption {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device) {
        SaneOption.assertType(cOption: cOption, type: SANE_TYPE_BOOL, class: SaneOptionBool.self)
        super.init(cOption: cOption, index: index, device: device)
    }
    
    public private(set) var value: Bool = false
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
}

extension SaneOptionBool : DeviceOptionTypedInternal {
    
    public func stringForValue(_ value: Bool, userFacing: Bool) -> String {
        if userFacing {
            return value ? "OPTION BOOL ON".saneTranslation : "OPTION BOOL OFF".saneTranslation
        }
        return value ? "On" : "Off"
    }
    
    public func bytesForValue(_ value: Bool) -> Data {
        var data = Data(repeating: 0, count: size)
        data.withUnsafeMutableBytes { (bytes: UnsafeMutablePointer<SANE_Bool>) -> () in
            bytes.pointee = value ? SANE_TRUE : SANE_FALSE
        }
        return data
    }
    
    internal func updateFromBytes(_ bytes: UnsafeRawPointer) {
        value = bytes.bindMemory(to: SANE_Bool.self, capacity: size).pointee == SANE_TRUE
    }
    
    public var constraint: OptionConstraint<Bool> {
        return .none
    }
    
    var bestPreviewValue: DeviceOptionNewValue<Bool> {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return .none }
        switch value {
        case .auto: return .auto
        case .on:   return .value(true)
        case .off:  return .value(false)
        default:    fatalError("Unsupported preview value \(value) for option \(identifier ?? "<nil>")")
        }
    }
}

// MARK: Int
public class SaneOptionInt: SaneOption {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device) {
        SaneOption.assertType(cOption: cOption, type: SANE_TYPE_INT, class: SaneOptionInt.self)
        super.init(cOption: cOption, index: index, device: device)
    }
    
    public private(set) var value: Int = 0
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
}

extension SaneOptionInt : DeviceOptionTypedInternal {
    
    public func stringForValue(_ value: Int, userFacing: Bool) -> String {
        let unitString = userFacing && unit != SANE_UNIT_NONE ? " " + unit.description : ""
        return String(value) + unitString
    }
    
    public func bytesForValue(_ value: Int) -> Data {
        var data = Data(repeating: 0, count: size)
        data.withUnsafeMutableBytes { (bytes: UnsafeMutablePointer<SANE_Int>) -> () in
            bytes.pointee = SANE_Int(value)
        }
        return data
    }
    
    internal func updateFromBytes(_ bytes: UnsafeRawPointer) {
        let saneValue = bytes.bindMemory(to: SANE_Int.self, capacity: size).pointee
        value = Int(saneValue)
    }
    
    public var constraint: OptionConstraint<Int> {
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
            fatalError("Unsupported constraint type for DeviceOptionNumber")
        }
    }
    
    var bestPreviewValue: DeviceOptionNewValue<Int> {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return .none }
        switch value {
        case .auto:
            return .auto
        case .min, .max:
            switch constraint {
            case .range(let min, let max), .stepRange(let min, let max, _, _):
                return value == .min ? .value(min) : .value(max)
            case .list(let values):
                guard !values.isEmpty else { return .none }
                return value == .min ? .value(values.min()!) : .value(values.max()!)
            default:
                return .none
            }
        default:
            fatalError("Unsupported preview value \(value) for option \(identifier ?? "<nil>")")
        }
    }
}

// MARK: Fixed float
public class SaneOptionFixed: SaneOption {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device) {
        SaneOption.assertType(cOption: cOption, type: SANE_TYPE_FIXED, class: SaneOptionFixed.self)
        super.init(cOption: cOption, index: index, device: device)
    }
    
    public private(set) var value: Double = 0
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
}

extension SaneOptionFixed : DeviceOptionTypedInternal {
    
    public func stringForValue(_ value: Double, userFacing: Bool) -> String {
        let unitString = userFacing && unit != SANE_UNIT_NONE ? " " + unit.description : ""
        return String(value) + unitString
    }
    
    public func bytesForValue(_ value: Double) -> Data {
        var data = Data(repeating: 0, count: size)
        data.withUnsafeMutableBytes { (bytes: UnsafeMutablePointer<SANE_Fixed>) -> () in
            bytes.pointee = SaneFixedFromDouble(value)
        }
        return data
    }
    
    internal func updateFromBytes(_ bytes: UnsafeRawPointer) {
        let saneValue = bytes.bindMemory(to: SANE_Fixed.self, capacity: size).pointee
        value = SaneDoubleFromFixed(saneValue)
    }
    
    public var constraint: OptionConstraint<Double> {
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
            fatalError("Unsupported constraint type for DeviceOptionNumber")
        }
    }
    
    var bestPreviewValue: DeviceOptionNewValue<Double> {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return .none }
        switch value {
        case .auto:
            return .auto
        case .min, .max:
            switch constraint {
            case .range(let min, let max), .stepRange(let min, let max, _, _):
                return value == .min ? .value(min) : .value(max)
            case .list(let values):
                guard !values.isEmpty else { return .none }
                return value == .min ? .value(values.min()!) : .value(values.max()!)
            default:
                return .none
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
public class SaneOptionString: SaneOption {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device) {
        SaneOption.assertType(cOption: cOption, type: SANE_TYPE_STRING, class: SaneOptionString.self)
        super.init(cOption: cOption, index: index, device: device)
    }
    
    public private(set) var value: String = ""
    
    public override var localizedValue: String {
        return stringForValue(value, userFacing: true)
    }
}

extension SaneOptionString : DeviceOptionTypedInternal {
    
    public func stringForValue(_ value: String, userFacing: Bool) -> String {
        let unitString = userFacing && unit != SANE_UNIT_NONE ? " " + unit.description : ""
        let stringValue = userFacing ? value.saneTranslation : value
        return stringValue + unitString
    }
    
    public func bytesForValue(_ value: String) -> Data {
        // TODO: keep max size in mind!
        // let size = min(cString.count, option.size)
        return value.data(using: .utf8) ?? Data()
    }
    
    internal func updateFromBytes(_ bytes: UnsafeRawPointer) {
        let saneValue = bytes.bindMemory(to: SANE_Char.self, capacity: size)
        value = String(cString: saneValue)
    }
    
    public var constraint: OptionConstraint<String> {
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
            fatalError("Unsupported constraint type for DeviceOptionNumber")
        }
    }

    var bestPreviewValue: DeviceOptionNewValue<String> {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return .none }
        if value == .auto {
            return .auto
        }
        fatalError("Unsupported preview value \(value) for option \(identifier ?? "<nil>")")
    }
}

// MARK: Button
public class SaneOptionButton: SaneOption {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device) {
        SaneOption.assertType(cOption: cOption, type: SANE_TYPE_BUTTON, class: SaneOptionButton.self)
        super.init(cOption: cOption, index: index, device: device)
    }
    
    public func press() {
        // TODO: Sane.shared.setValueForOption(value: true, auto: false, option: self, completion: nil)
    }
}

// MARK: Button
public class SaneOptionGroup: SaneOption {
    override init(cOption: SANE_Option_Descriptor, index: Int, device: Device) {
        SaneOption.assertType(cOption: cOption, type: SANE_TYPE_GROUP, class: SaneOptionGroup.self)
        super.init(cOption: cOption, index: index, device: device)
    }
    
    var options: [SaneOption] {
        // TODO: implement
        //let nextGroupIndex = device.options.filter {  }
        //return device.options.}
        return []
    }
}
