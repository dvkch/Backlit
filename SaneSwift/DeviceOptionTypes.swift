//
//  DeviceOption.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//


// TODO: use associated types?
// TODO: value can be updated from inside the refreshValue method + setValueForOption. this should not happen!

// MARK: Bool
public class DeviceOptionBool : DeviceOption {
    public override init(cOpt: SANE_Option_Descriptor, index: Int, device: Device) {
        guard cOpt.type == SANE_TYPE_BOOL else {
            fatalError("DeviceOptionBool should only be used with SANE_TYPE_BOOL option type")
        }
        super.init(cOpt: cOpt, index: index, device: device)
    }
    
    // MARK: Properties
    public internal(set) var value: Bool = false
    
    // MARK: Overrides
    override func refreshValue(_ block: ((Error?) -> ())?) {
        guard capabilities.isActive else {
            block?(nil)
            return
        }
        
        Sane.shared.valueForOption(self) { (value, error) in
            if let value = value as? Bool {
                self.value = value
            }
            block?(error)
        }
    }
    
    public override func stringForValue(_ value: Any?, userFacing: Bool) -> String {
        let boolValue = (value as? Bool) ?? false
        if userFacing {
            return boolValue ? "OPTION BOOL ON".saneTranslation : "OPTION BOOL OFF".saneTranslation
        }
        return boolValue ? "On" : "Off"
    }
    
    public override func valueString(userFacing: Bool) -> String {
        guard capabilities.isActive else { return "" }
        return stringForValue(value, userFacing: userFacing)
    }
}


// MARK: Button
public class DeviceOptionButton : DeviceOption {
    public override init(cOpt: SANE_Option_Descriptor, index: Int, device: Device) {
        guard cOpt.type == SANE_TYPE_BUTTON else {
            fatalError("DeviceOptionButton should only be used with SANE_TYPE_BUTTON option type")
        }
        super.init(cOpt: cOpt, index: index, device: device)
    }
    
    // MARK: Methods
    public func press(_ completion: ((_ reloadAll: Bool, _ error: Error?) -> ())?) {
        Sane.shared.setValueForOption(value: true, auto: false, option: self, completion: completion)
    }
    
    // MARK: Overrides
    public override func stringForValue(_ value: Any?, userFacing: Bool) -> String {
        return ""
    }
    
    public override func valueString(userFacing: Bool) -> String {
        return ""
    }
    
    override func refreshValue(_ block: ((Error?) -> ())?) {
        block?(nil)
    }
}


// MARK: Group
public class DeviceOptionGroup : DeviceOption {
    public override init(cOpt: SANE_Option_Descriptor, index: Int, device: Device) {
        guard cOpt.type == SANE_TYPE_GROUP else {
            fatalError("DeviceOptionGroup should only be used with SANE_TYPE_GROUP option type")
        }
        super.init(cOpt: cOpt, index: index, device: device)
    }
    
    // MARK: Methods
    public internal(set) var items = [DeviceOption]()
    
    // MARK: Overrides
    public override func stringForValue(_ value: Any?, userFacing: Bool) -> String {
        return ""
    }
    
    public override func valueString(userFacing: Bool) -> String {
        return ""
    }
    
    override func refreshValue(_ block: ((Error?) -> ())?) {
        block?(nil)
    }
}

// MARK: String
public class DeviceOptionString : DeviceOption {
    public override init(cOpt: SANE_Option_Descriptor, index: Int, device: Device) {
        guard cOpt.type == SANE_TYPE_STRING else {
            fatalError("DeviceOptionString should only be used with SANE_TYPE_STRING option type")
        }
        if cOpt.constraint_type == SANE_CONSTRAINT_STRING_LIST {
            var values = [String]()
            while let value = cOpt.constraint.string_list.advanced(by: values.count).pointee {
                values.append(value.asString() ?? "")
            }
            constraintValues = values
        }
        else {
            constraintValues = nil
        }
        super.init(cOpt: cOpt, index: index, device: device)
    }
    
    // MARK: Methods
    public let constraintValues: [String]?
    public internal(set) var value: String?
    
    public func constraintValues(userFacing: Bool) -> [String]? {
        return constraintValues?.map { stringForValue($0, userFacing: userFacing) }
    }
    
    // MARK: Overrides
    public override var readOnlyOrSingleOption: Bool {
        if super.readOnlyOrSingleOption { return true }
        
        switch constraintType {
        case SANE_CONSTRAINT_NONE:          return false
        case SANE_CONSTRAINT_STRING_LIST:   return (constraintValues?.count ?? 0) <= 1
        default: fatalError("Unsupported constraint for string option")
        }
    }
    
    public override func stringForValue(_ value: Any?, userFacing: Bool) -> String {
        var stringValue = (value as? String)
        
        if userFacing {
            stringValue = stringValue?.saneTranslation
        }
        
        var parts = [stringValue]
        if unit != SANE_UNIT_NONE && userFacing {
            parts.append(unit.description)
        }
        
        return parts.compactMap({ $0 }).joined(separator: " ")
    }
    
    public override func valueString(userFacing: Bool) -> String {
        guard capabilities.isActive else { return "" }
        return stringForValue(value, userFacing: userFacing)
    }
    
    override func refreshValue(_ block: ((Error?) -> ())?) {
        guard capabilities.isActive else {
            block?(nil)
            return
        }
        
        Sane.shared.valueForOption(self) { (value, error) in
            if error == nil {
                self.value = value as? String
            }
            block?(error)
        }
    }
    
    public override var descriptionConstraint: String {
        if constraintType == SANE_CONSTRAINT_STRING_LIST {
            return String(format: "OPTION CONSTRAINED LIST %@".saneTranslation, (constraintValues ?? []).joined(separator: ", "))
        }
        
        return "OPTION CONSTRAINED NOT CONSTRAINED".saneTranslation
    }
}

// MARK: Number
public class DeviceOptionNumber : DeviceOption {
    public override init(cOpt: SANE_Option_Descriptor, index: Int, device: Device) {
        guard cOpt.type == SANE_TYPE_INT || cOpt.type == SANE_TYPE_FIXED else {
            fatalError("DeviceOptionNumber should only be used with SANE_TYPE_FIXED or SANE_TYPE_INT option type")
        }
        
        if cOpt.constraint_type == SANE_CONSTRAINT_WORD_LIST {
            let count = cOpt.constraint.word_list!.pointee
            constraintValues = (0..<Int(count)).map { i in
                if cOpt.type == SANE_TYPE_INT {
                    return NSNumber(value: cOpt.constraint.word_list.advanced(by: i+1).pointee)
                }
                return NSNumber(value: SaneDoubleFromFixed(cOpt.constraint.word_list.advanced(by: i+1).pointee))
            }
            minValue = nil
            maxValue = nil
            stepValue = nil
        }
        else if cOpt.constraint_type == SANE_CONSTRAINT_RANGE {
            constraintValues = nil
            
            if cOpt.type == SANE_TYPE_INT {
                minValue = NSNumber(value: cOpt.constraint.range.pointee.min)
                maxValue = NSNumber(value: cOpt.constraint.range.pointee.max)
                if cOpt.constraint.range.pointee.quant != 0 {
                    stepValue = NSNumber(value: cOpt.constraint.range.pointee.quant)
                } else {
                    stepValue = nil
                }
            }
            else {
                minValue = NSNumber(value: SaneDoubleFromFixed(cOpt.constraint.range.pointee.min))
                maxValue = NSNumber(value: SaneDoubleFromFixed(cOpt.constraint.range.pointee.max))
                if cOpt.constraint.range.pointee.quant != 0 {
                    stepValue = NSNumber(value: SaneDoubleFromFixed(cOpt.constraint.range.pointee.quant))
                } else {
                    stepValue = nil
                }
            }
        }
        else {
            minValue = nil
            maxValue = nil
            stepValue = nil
            constraintValues = nil
        }
        super.init(cOpt: cOpt, index: index, device: device)
    }
    
    // MARK: Methods
    public let constraintValues: [NSNumber]?

    public func constraintValues(userFacing: Bool) -> [String]? {
        return constraintValues?.map { stringForValue($0, userFacing: userFacing) }
    }
    
    public internal(set) var value: NSNumber?
    public let minValue: NSNumber?
    public let maxValue: NSNumber?
    public let stepValue: NSNumber?
    
    public var rangeValues: [NSNumber]? {
        guard let min = self.minValue, let max = self.maxValue, let step = self.stepValue else { return nil }
        
        if type == SANE_TYPE_INT {
            return stride(from: min.intValue, to: max.intValue, by: step.intValue).map { NSNumber(value: $0) }
        }
        return stride(from: min.doubleValue, to: max.doubleValue, by: step.doubleValue).map { NSNumber(value: $0) }
    }
    
    public var bestValueForPreview: NSNumber? {
        guard let value = SaneStandardOption(saneIdentifier: identifier)?.bestPreviewValue else { return nil }
        if value == .auto {
            return nil
        }
        
        if constraintType == SANE_CONSTRAINT_RANGE {
            return value == .max ? maxValue : minValue
        }
        
        if constraintType == SANE_CONSTRAINT_WORD_LIST {
            let sortedValues = constraintValues?.sorted { $0.compare($1) == .orderedAscending }
            return value == .max ? sortedValues?.last : sortedValues?.first
        }
        
        return self.value
    }

    // MARK: Overrides
    public override func stringForValue(_ value: Any?, userFacing: Bool) -> String {
        var unitString = ""
        if unit != SANE_UNIT_NONE && userFacing {
            unitString = " " + unit.description
        }
        
        if type == SANE_TYPE_INT {
            let v = (value as? NSNumber)?.intValue ?? 0
            return String(v) + unitString
        }
        
        let v = (value as? NSNumber)?.doubleValue ?? 0
        return String(format: "%.02lf", v) + unitString
    }
    
    public override func valueString(userFacing: Bool) -> String {
        return capabilities.isActive ? stringForValue(value, userFacing: userFacing) : ""
    }
    
    override func refreshValue(_ block: ((Error?) -> ())?) {
        guard capabilities.isActive else {
            block?(nil)
            return
        }
        
        Sane.shared.valueForOption(self) { (value, error) in
            if error == nil {
                if self.type == SANE_TYPE_INT {
                    self.value = value as? NSNumber
                } else {
                    // TODO: SANE_UNFIX?
                    self.value = NSNumber(value: SaneDoubleFromFixed((value as? NSNumber)?.int32Value ?? 0))
                }
            }
            block?(error)
        }
    }
    
    public override var readOnlyOrSingleOption: Bool {
        if super.readOnlyOrSingleOption { return true }
        
        switch constraintType {
        case SANE_CONSTRAINT_NONE:
            return false
        case SANE_CONSTRAINT_RANGE:
            if stepValue == nil {
                return minValue == maxValue
            }
            return (rangeValues?.count ?? 0) <= 1
        case SANE_CONSTRAINT_WORD_LIST:
            return (constraintValues?.count ?? 0) <= 1
        default:
            fatalError("Unsupported constraint type for DeviceOptionNumber")
        }
    }
    
    public override var descriptionConstraint: String {
        if constraintType == SANE_CONSTRAINT_RANGE {
            if let stepValue = self.stepValue {
                return String(format: "OPTION CONSTRAINED RANGE FROM TO STEP %@ %@ %@".saneTranslation,
                              self.stringForValue(minValue, userFacing: true),
                              self.stringForValue(maxValue, userFacing: true),
                              self.stringForValue(stepValue, userFacing: true))
            }
            else {
                return String(format: "OPTION CONSTRAINED RANGE FROM TO %@ %@".saneTranslation,
                              self.stringForValue(minValue, userFacing: true),
                              self.stringForValue(maxValue, userFacing: true))
            }
        }
        else if constraintType == SANE_CONSTRAINT_WORD_LIST {
            let values = constraintValues(userFacing: true)?.joined(separator: ", ") ?? ""
            return String(format: "OPTION CONSTRAINED LIST %@".saneTranslation, values)
        }
        return "OPTION CONSTRAINED NOT CONSTRAINED".saneTranslation
    }
}

