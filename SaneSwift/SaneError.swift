//
//  SaneError.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

public enum SaneError: Error {
    case cancelled
    case saneError(_ status: SANE_Status)
    case deviceNotOpened
    case couldntGetOptions
    case getValueForButtonType
    case getValueForGroupType
    case setValueForGroupType
    case noImageData
    case cannotGenerateImage
    case unsupportedChannels
    
    public init(saneStatus: SANE_Status) {
        if saneStatus == SANE_STATUS_CANCELLED {
            self = .cancelled
        } else {
            self = .saneError(saneStatus)
        }
    }
    
    public var saneStatus: SANE_Status? {
        switch self {
        case .saneError(let status): return status
        default: return nil
        }
    }
    
    public var isSaneAuthDenied: Bool {
        if case .saneError(let status) = self, status == SANE_STATUS_ACCESS_DENIED {
            return true
        }
        return false
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
        case .couldntGetOptions:            return 2
        case .getValueForButtonType:        return 3
        case .getValueForGroupType:         return 4
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
        case .saneError(let status):        return "ERROR MESSAGE SANE ERROR".saneTranslation + "\n" + status.description.saneTranslation
        case .deviceNotOpened:              return "ERROR MESSAGE DEVICE NOT OPENED".saneTranslation
        case .couldntGetOptions:            return "ERROR MESSAGE COULDNT GET OPTIONS".saneTranslation
        case .getValueForButtonType:        return "ERROR MESSAGE GET VALUE TYPE BUTTON".saneTranslation
        case .getValueForGroupType:         return "ERROR MESSAGE GET VALUE TYPE GROUP".saneTranslation
        case .setValueForGroupType:         return "ERROR MESSAGE SET VALUE TYPE GROUP".saneTranslation
        case .noImageData:                  return "ERROR MESSAGE NO IMAGE DATA".saneTranslation
        case .cannotGenerateImage:          return "ERROR MESSAGE CANNOT GENERATE IMAGE".saneTranslation
        case .unsupportedChannels:          return "ERROR MESSAGE UNSUPPORTED CHANNELS".saneTranslation
        }
    }
}
