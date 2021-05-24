//
//  SaneTypes.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 20/02/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

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
        case SANE_TYPE_BOOL:    return "SANE_TYPE_BOOL"
        case SANE_TYPE_BUTTON:  return "SANE_TYPE_BUTTON"
        case SANE_TYPE_FIXED:   return "SANE_TYPE_FIXED"
        case SANE_TYPE_GROUP:   return "SANE_TYPE_GROUP"
        case SANE_TYPE_INT:     return "SANE_TYPE_INT"
        case SANE_TYPE_STRING:  return "SANE_TYPE_STRING"
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
public struct SaneCapabilities: OptionSet {
    public var rawValue: SANE_Int
    
    public init(rawValue: SANE_Int) {
        self.rawValue = rawValue
    }

    public static let softwareSettable = SaneCapabilities(rawValue: SANE_CAP_SOFT_SELECT)
    public static let hardwareSettable = SaneCapabilities(rawValue: SANE_CAP_HARD_SELECT)
    public static let readable         = SaneCapabilities(rawValue: SANE_CAP_SOFT_DETECT)
    public static let emulated         = SaneCapabilities(rawValue: SANE_CAP_EMULATED)
    public static let automatic        = SaneCapabilities(rawValue: SANE_CAP_AUTOMATIC)
    public static let inactive         = SaneCapabilities(rawValue: SANE_CAP_INACTIVE)
    public static let advanced         = SaneCapabilities(rawValue: SANE_CAP_ADVANCED)
    
    public var isActive: Bool {
        return !contains(.inactive)
    }
    
    public var isSettable: Bool {
        return contains(.softwareSettable)
    }
}

extension SaneCapabilities: CustomStringConvertible {
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
public struct SaneInfo: OptionSet {
    public var rawValue: SANE_Int
    
    public init(rawValue: SANE_Int) {
        self.rawValue = rawValue
    }

    public static let inexact          = SaneInfo(rawValue: SANE_INFO_INEXACT)
    public static let reloadOptions    = SaneInfo(rawValue: SANE_INFO_RELOAD_OPTIONS)
    public static let reloadScanParams = SaneInfo(rawValue: SANE_INFO_RELOAD_PARAMS)
}

// MARK: Standard options
public enum SaneStandardOption: CaseIterable {
    case preview, resolution, resolutionX, resolutionY, colorMode, areaTopLeftX, areaTopLeftY, areaBottomRightX, areaBottomRightY, imageIntensity
    
    var saneIdentifier: String {
        switch self {
        case .preview:          return SANE_NAME_PREVIEW
        case .resolution:       return SANE_NAME_SCAN_RESOLUTION
        case .resolutionX:      return SANE_NAME_SCAN_X_RESOLUTION
        case .resolutionY:      return SANE_NAME_SCAN_Y_RESOLUTION
        case .colorMode:        return SANE_NAME_SCAN_MODE
        case .areaTopLeftX:     return SANE_NAME_SCAN_TL_X
        case .areaTopLeftY:     return SANE_NAME_SCAN_TL_Y
        case .areaBottomRightX: return SANE_NAME_SCAN_BR_X
        case .areaBottomRightY: return SANE_NAME_SCAN_BR_Y
        case .imageIntensity:   return SANE_NAME_GAMMA_VECTOR
        }
    }
    
    init?(saneIdentifier: String?) {
        guard let option = SaneStandardOption.allCases.first(where: { $0.saneIdentifier == saneIdentifier }) else { return nil }
        self = option
    }
    
    static var cropOptions: [SaneStandardOption] {
        return [.areaTopLeftX, .areaTopLeftY, .areaBottomRightX, .areaBottomRightY]
    }
    
    public enum PreviewValue {
        case auto, on, off, min, max
    }
    
    var bestPreviewValue: PreviewValue {
        switch self {
        case .preview:                                  return .on
        case .colorMode, .imageIntensity:               return .auto
        case .resolution, .resolutionX, .resolutionY:   return .min
        case .areaTopLeftX, .areaTopLeftY:              return .min
        case .areaBottomRightX, .areaBottomRightY:      return .max
        }
    }
}

// MARK: Standard color mode values
public extension SaneValueScanMode {
    var value: String? {
        return NSStringFromSaneValueScanMode(self)
    }
}

// MARK: Scan status
public enum ScanOperation {
    case scan, preview
}

public enum ScanProgress: Equatable {
    case warmingUp
    case scanning(progress: Float, incompletePreview: UIImage?, estimatedParameters: ScanParameters?)
    case cancelling

    var image: UIImage? {
        if case .scanning(_, let image, _) = self {
            return image
        }
        return nil
    }
}

public typealias ScanResult = Result<(UIImage, ScanParameters), Error>

public extension ScanResult {
    var image: UIImage? {
        if case .success(let data) = self {
            return data.0
        }
        return nil
    }
}
