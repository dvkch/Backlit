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
        self.identifier     = cOpt.name.asString()
        self.localizedTitle = Sane.shared.translation(for: cOpt.title.asString() ?? "")
        self.localizedDescr = Sane.shared.translation(for: cOpt.desc.asString()  ?? "")
        self.type                   = cOpt.type
        self.unit                   = cOpt.unit
        self.size                   = Int(cOpt.size)
        self.capabilities           = SaneCap(rawValue: cOpt.cap)
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
    public let capabilities: SaneCap
    public let constraintType: SANE_Constraint_Type

    public var disabledOrReadOnly: Bool {
        return capabilities.contains(.inactive) || !capabilities.contains(.softwareSettable)
    }
    public var readOnlyOrSingleOption: Bool {
        return capabilities.contains(.inactive) || !capabilities.contains(.softwareSettable)
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
    
    public var debugDescriptionHuman: String {
        return ["#\(index)",
            localizedTitle,
            type.description,
            unit.description,
            capabilities.description,
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
        return "\(Swift.type(of: self)): \(index), \(localizedTitle), \(type.description), \(unit.description)"
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
