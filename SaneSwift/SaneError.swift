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
    
    static func fromStatus(_ status: SANE_Status, expected: SANE_Status = SANE_STATUS_GOOD) -> SaneError? {
        if status == expected {
            return nil
        }
        if status == SANE_STATUS_CANCELLED {
            return .cancelled
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
        switch self {
        case .cancelled:                    return "ERROR MESSAGE USER CANCELLED".saneTranslation
        case .saneError(let status):        return "ERROR MESSAGE SANE ERROR".saneTranslation + "\n" + status.description
        case .deviceNotOpened:              return "ERROR MESSAGE DEVICE NOT OPENED".saneTranslation
        case .getValueForButtonType:        return "ERROR MESSAGE GET VALUE TYPE BUTTON".saneTranslation
        case .getValueForGroupType:         return "ERROR MESSAGE GET VALUE TYPE GROUP".saneTranslation
        case .getValueForInactiveOption:    return "ERROR MESSAGE GET VALUE INACTIVE OPTION".saneTranslation
        case .setValueForGroupType:         return "ERROR MESSAGE SET VALUE TYPE GROUP".saneTranslation
        case .noImageData:                  return "ERROR MESSAGE NO IMAGE DATA".saneTranslation
        case .cannotGenerateImage:          return "ERROR MESSAGE CANNOT GENERATE IMAGE".saneTranslation
        case .unsupportedChannels:          return "ERROR MESSAGE UNSUPPORTED CHANNELS".saneTranslation
        }
    }
}
