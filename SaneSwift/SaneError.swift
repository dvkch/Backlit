//
//  SaneError.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

public enum SaneError: Error {
    case cancelled
    case saneError(_ status: SANE_Status)
    case deviceNotOpened
    case getValueForButtonType
    case getValueForGroupType
    case getValueForInactiveOption
    case setValueForGroupType
    case noImageData
    case cannotGenerateImage
    case unsupportedChannels
    
    static func fromStatus(_ status: SANE_Status) -> SaneError? {
        if status == SANE_STATUS_CANCELLED {
            return .cancelled
        }
        if status == SANE_STATUS_GOOD {
            return nil
        }
        return .saneError(status)
    }
}

extension SaneError : CustomNSError {
    public static var errorDomain: String {
        return "SaneErrorDomain"
    }
    
    public var errorCode: Int {
        switch self {
        case .cancelled:                    return 0
        case .saneError(let status):        return 1000 + Int(status.rawValue)
        case .deviceNotOpened:              return 1
        case .getValueForButtonType:        return 2
        case .getValueForGroupType:         return 3
        case .getValueForInactiveOption:    return 4
        case .setValueForGroupType:         return 5
        case .noImageData:                  return 6
        case .cannotGenerateImage:          return 7
        case .unsupportedChannels:          return 8
        }
    }
    
    public var errorUserInfo: [String: Any] {
        return [:]
    }
}

extension SaneError: LocalizedError {
    public var errorDescription: String? {
        // TODO: translate
        switch self {
        case .cancelled:                    return "ERROR MESSAGE USER CANCELLED"
        case .saneError(let status):        return "ERROR MESSAGE SANE ERROR" + "\n" + status.description
        case .deviceNotOpened:              return "ERROR MESSAGE DEVICE NOT OPENED"
        case .getValueForButtonType:        return "ERROR MESSAGE GET VALUE TYPE BUTTON"
        case .getValueForGroupType:         return "ERROR MESSAGE GET VALUE TYPE GROUP"
        case .getValueForInactiveOption:    return "ERROR MESSAGE GET VALUE INACTIVE OPTION"
        case .setValueForGroupType:         return "ERROR MESSAGE SET VALUE TYPE GROUP"
        case .noImageData:                  return "ERROR MESSAGE NO IMAGE DATA"
        case .cannotGenerateImage:          return "ERROR MESSAGE CANNOT GENERATE IMAGE"
        case .unsupportedChannels:          return "ERROR MESSAGE UNSUPPORTED CHANNELS"
        }
    }
}
