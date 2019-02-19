//
//  DeviceOption.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

public class DeviceOption {
    
    // MARK: Init
    static func bestOption(cOpt: SANE_Option_Descriptor, index: Int, device: Device) -> DeviceOption {
        switch cOpt.type {
        case SANE_TYPE_BOOL:    return DeviceOptionBool(cOpt: cOpt, index: index, device: device)
        case SANE_TYPE_INT:     return DeviceOptionNumber(cOpt: cOpt, index: index, device: device)
        case SANE_TYPE_FIXED:   return DeviceOptionNumber(cOpt: cOpt, index: index, device: device)
        case SANE_TYPE_STRING:  return DeviceOptionString(cOpt: cOpt, index: index, device: device)
        case SANE_TYPE_BUTTON:  return DeviceOptionButton(cOpt: cOpt, index: index, device: device)
        case SANE_TYPE_GROUP:   return DeviceOptionGroup(cOpt: cOpt, index: index, device: device)
        default: fatalError("Unsupported type: \(cOpt.type)")
        }
    }

    init(cOpt: SANE_Option_Descriptor, index: Int, device: Device) {
        self.index          = index
        self.device         = device
        self.identifier     = NSStringFromSaneString(cOpt.name)
        self.localizedTitle = Sane.shared.translation(for: NSStringFromSaneString(cOpt.title) ?? "")
        self.localizedDescr = Sane.shared.translation(for: NSStringFromSaneString(cOpt.desc) ?? "")
        self.type                   = cOpt.type
        self.unit                   = cOpt.unit
        self.size                   = Int(cOpt.size)
        self.cap                    = cOpt.cap
        self.capReadable            = cOpt.cap & SANE_CAP_SOFT_DETECT > 0
        self.capSetAuto             = cOpt.cap & SANE_CAP_AUTOMATIC > 0
        self.capEmulated            = cOpt.cap & SANE_CAP_EMULATED > 0
        self.capInactive            = cOpt.cap & SANE_CAP_INACTIVE > 0
        self.capAdvanced            = cOpt.cap & SANE_CAP_ADVANCED > 0
        self.capSettableViaSoftware = cOpt.cap & SANE_CAP_SOFT_SELECT > 0
        self.capSettableViaHardware = cOpt.cap & SANE_CAP_HARD_SELECT > 0
        self.constraintType         = cOpt.constraint_type
    }
    
    // MARK: Properties
    public let index: Int
    public let device: Device // TODO: needed?
    public let identifier: String?
    public let localizedTitle: String
    public let localizedDescr: String
    public let type: SANE_Value_Type
    public let unit: SANE_Unit
    public let size: Int
    private let cap: SANE_Int
    public var capSetAuto: Bool
    public var capReadable: Bool
    public var capEmulated: Bool
    public var capInactive: Bool
    public var capAdvanced: Bool
    public var capSettableViaSoftware: Bool
    public var capSettableViaHardware: Bool
    public let constraintType: SANE_Constraint_Type

    public var disabledOrReadOnly: Bool {
        return !capSettableViaSoftware || capInactive
    }
    public var readOnlyOrSingleOption: Bool {
        return !capSettableViaSoftware || capInactive
    }

    public var descriptionConstraint: String {
        // TODO: translate
        switch constraintType {
        case SANE_CONSTRAINT_RANGE:         return "OPTION CONSTRAINED RANGE"
        case SANE_CONSTRAINT_STRING_LIST:   return "OPTION CONSTRAINED LIST"
        case SANE_CONSTRAINT_WORD_LIST:     return "OPTION CONSTRAINED LIST"
        case SANE_CONSTRAINT_NONE:          return "OPTION CONSTRAINED NOT CONSTRAINED"
        default: fatalError("Unknown constraint type: \(constraintType)")
        }
    }
    
    public var debugDescriptionCapabilities: String {
        var descriptions = [String]()
        
        if capReadable {
            descriptions.append("readable")
        } else {
            descriptions.append("not readable")
        }
        
        if capSettableViaSoftware {
            descriptions.append("settable via software")
        }
        if capSettableViaHardware {
            descriptions.append("settable via hardware")
        }
        if !capSettableViaHardware && !capSettableViaSoftware {
            descriptions.append("not settable")
        }
        
        if capSetAuto {
            descriptions.append("has auto value")
        }
        
        if capInactive {
            descriptions.append("inactive")
        }
        
        if capAdvanced {
            descriptions.append("advanced")
        }
        
        if capEmulated {
            descriptions.append("emulated")
        }
        
        return descriptions.joined(separator: ", ")
    }
    
    public var debugDescriptionHuman: String {
        return ["#\(index)",
            localizedTitle,
            NSStringFromSANE_Value_Type(type: type),
            NSStringFromSANE_Unit(unit: unit),
            debugDescriptionCapabilities,
            descriptionConstraint
            ].joined(separator: ", ")
    }

    public func valueString(withUnit: Bool) -> String {
        fatalError("Not implemented")
    }
    
    public func stringForValue(_ value: Any?, withUnit: Bool) -> String {
        fatalError("Not implemented")
    }
    
    // TODO: public?
    func refreshValue(_ block: ((Error?) -> ())?) {
        fatalError("Not implemented")
    }
}

extension DeviceOption: CustomStringConvertible {
    public var description: String {
        return "\(Swift.type(of: self)): \(index), \(localizedTitle), \(NSStringFromSANE_Value_Type(type: type)), \(NSStringFromSANE_Unit(unit: unit))"
    }
}

extension DeviceOption {
    static func groupedOptions(_ options: [DeviceOption], removeEmpty: Bool) -> [DeviceOptionGroup] {
        var groups = [DeviceOptionGroup]()
        
        for option in options {
            if let group = option as? DeviceOptionGroup {
                group.items = []
                groups.append(group)
                continue
            }
            groups.last?.items.append(option)
        }
        
        if removeEmpty {
            groups = groups.filter { !$0.items.isEmpty }
        }
        
        return groups
    }
}

func NSStringFromSANE_Value_Type(type: SANE_Value_Type) -> String {
    switch type {
    case SANE_TYPE_BOOL:    return "bool"
    case SANE_TYPE_BUTTON:  return "button"
    case SANE_TYPE_FIXED:   return "fixed"
    case SANE_TYPE_GROUP:   return "group"
    case SANE_TYPE_INT:     return "int"
    case SANE_TYPE_STRING:  return "string"
    default: fatalError("Unknown type: \(type)")
    }
}

func NSStringFromSANE_Unit(unit: SANE_Unit) -> String {
    switch unit {
    case SANE_UNIT_NONE:        return "none"
    case SANE_UNIT_PIXEL:       return "px"
    case SANE_UNIT_BIT:         return "bits"
    case SANE_UNIT_MM:          return "mm"
    case SANE_UNIT_DPI:         return "dpi"
    case SANE_UNIT_PERCENT:     return "%"
    case SANE_UNIT_MICROSECOND: return "ms"
    default: fatalError("Unknown unit: \(unit)")
    }
}
