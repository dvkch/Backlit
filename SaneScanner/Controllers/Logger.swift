//
//  Logger.swift
//  SaneScanner
//
//  Created by syan on 06/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation
import OSLog

struct Logger {
    private init() {}
    
    // MARK: Types
    enum Tag: String {
        case app = "App"
        case bonjour = "Bonjour"
        case gallery = "Gallery"
        case prefs = "Prefs"
        case sane = "SANE"
        
        @available(iOS 12.0, *)
        var asOSLog: OSLog {
            return OSLog(subsystem: Bundle.main.bundleIdentifier!, category: rawValue)
        }
    }
    
    public static var level: OSLogType = .info
    
    // MARK: Logging
    static func log(level: OSLogType, tag: Tag, _ message: String) {
        guard level.value >= self.level.value else { return }
        if #available(iOS 12.0, *) {
            os_log(level, log: tag.asOSLog, "%@", message)
        } else {
            print("[\(tag.rawValue)] [\(level.value)] \(message)")
        }
    }
    
    static func d(_ tag: Tag, _ message: String) {
        log(level: .debug, tag: tag, message)
    }
    
    static func i(_ tag: Tag, _ message: String) {
        log(level: .info, tag: tag, message)
    }
    
    static func w(_ tag: Tag, _ message: String) {
        log(level: .default, tag: tag, message)
    }
    
    static func e(_ tag: Tag, _ message: String) {
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
            return 10
        }
    }
}
