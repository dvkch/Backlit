//
//  SaneTypes.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 20/02/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

// MARK: CustomStringConvertibale
extension SANE_Frame : CustomStringConvertible {
    public var description: String {
        switch self {
        case SANE_FRAME_RGB:    return "RGB"
        case SANE_FRAME_RED:    return "R"
        case SANE_FRAME_GREEN:  return "G"
        case SANE_FRAME_BLUE:   return "B"
        case SANE_FRAME_GRAY:   return "GRAY"
        default: fatalError("Unknown frame type: \(self.rawValue)")
        }
    }
}

extension SANE_Value_Type: CustomStringConvertible {
    public var description: String {
        switch self {
        case SANE_TYPE_BOOL:    return "bool"
        case SANE_TYPE_BUTTON:  return "button"
        case SANE_TYPE_FIXED:   return "fixed"
        case SANE_TYPE_GROUP:   return "group"
        case SANE_TYPE_INT:     return "int"
        case SANE_TYPE_STRING:  return "string"
        default: fatalError("Unknown type: \(self.rawValue)")
        }
    }
}

extension SANE_Unit: CustomStringConvertible {
    public var description: String {
        switch self {
        case SANE_UNIT_NONE:        return "none"
        case SANE_UNIT_PIXEL:       return "px"
        case SANE_UNIT_BIT:         return "bits"
        case SANE_UNIT_MM:          return "mm"
        case SANE_UNIT_DPI:         return "dpi"
        case SANE_UNIT_PERCENT:     return "%"
        case SANE_UNIT_MICROSECOND: return "ms"
        default: fatalError("Unknown unit: \(self.rawValue)")
        }
    }
}

extension SANE_Status: CustomStringConvertible {
    public var description: String {
        return sane_strstatus(self)?.asString() ?? ""
    }
}

// MARK: SANE_String_Const
extension UnsafePointer where Pointee == SANE_Char {
    func asString() -> String? {
        return String(cString: self, encoding: .utf8)
    }
}

// MARK: Capabilities
public struct SaneCap: OptionSet {
    public var rawValue: SANE_Int
    
    public init(rawValue: SANE_Int) {
        self.rawValue = rawValue
    }

    public static let softwareSettable = SaneCap(rawValue: SANE_CAP_SOFT_SELECT)
    public static let hardwareSettable = SaneCap(rawValue: SANE_CAP_HARD_SELECT)
    public static let readable         = SaneCap(rawValue: SANE_CAP_SOFT_DETECT)
    public static let emulated         = SaneCap(rawValue: SANE_CAP_EMULATED)
    public static let automatic        = SaneCap(rawValue: SANE_CAP_AUTOMATIC)
    public static let inactive         = SaneCap(rawValue: SANE_CAP_INACTIVE)
    public static let advanced         = SaneCap(rawValue: SANE_CAP_ADVANCED)
    
    public var isActive: Bool {
        return !contains(.inactive)
    }
    
    public var isSettable: Bool {
        return contains(.softwareSettable)
    }
}

extension SaneCap: CustomStringConvertible {
    public var description: String {
        var descriptions = [String]()
        
        if contains(.readable)  { descriptions.append("readable") }
        else                    { descriptions.append("not readable") }
        
        if contains(.softwareSettable) { descriptions.append("settable via software") }
        if contains(.hardwareSettable) { descriptions.append("settable via hardware") }
        if !contains(.hardwareSettable) && !contains(.softwareSettable) { descriptions.append("not settable") }
        
        if contains(.automatic) { descriptions.append("automatic") }
        if contains(.automatic) { descriptions.append("inactive") }
        if contains(.advanced)  { descriptions.append("advanced") }
        if contains(.emulated)  { descriptions.append("emulated") }
        
        return descriptions.joined(separator: ", ")
    }
}

// MARK: Option info
struct SaneInfo: OptionSet {
    var rawValue: SANE_Int
    
    init(rawValue: SANE_Int) {
        self.rawValue = rawValue
    }

    static let inexact          = SaneInfo(rawValue: SANE_INFO_INEXACT)
    static let reloadOptions    = SaneInfo(rawValue: SANE_INFO_RELOAD_OPTIONS)
    static let reloadParams     = SaneInfo(rawValue: SANE_INFO_RELOAD_PARAMS)
}
