//
//  SaneLogger.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation
import OSLog

public struct SaneLogger {
    private init() {}

    internal enum Tag: String {
        case sane = "SANE"
        
        @available(iOS 12.0, *)
        var asOSLog: OSLog {
            return OSLog(subsystem: Bundle.main.bundleIdentifier!, category: rawValue)
        }
    }
    
    public static var level: OSLogType = .info
    
    private static func log(level: OSLogType, tag: Tag, _ message: String) {
        guard level.value >= self.level.value else { return }
        if #available(iOS 12.0, *) {
            os_log(level, log: tag.asOSLog, "%@", message)
        } else {
            print("[\(level.value)] \(message)")
        }
    }
    
    internal static func d(_ tag: Tag, _ message: String) {
        log(level: .debug, tag: tag, message)
    }

    internal static func i(_ tag: Tag, _ message: String) {
        log(level: .info, tag: tag, message)
    }
    
    internal static func w(_ tag: Tag, _ message: String) {
        log(level: .default, tag: tag, message)
    }

    internal static func e(_ tag: Tag, _ message: String) {
        log(level: .error, tag: tag, message)
    }
}

private extension OSLogType {
    var value: Int {
        switch self {
        case .debug:    return 0
        case .info:     return 1
        case .default:  return 2
        case .error:    return 3
        case .fault:    return 4
        default:
            return 0
        }
    }
}
