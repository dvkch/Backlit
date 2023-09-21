//
//  Logger+Sane.swift
//  Backlit
//
//  Created by syan on 10/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation
import SaneSwift
import OSLog

extension Logger {
    static func logSane(level: SaneLogger.Level, message: String) {
        let osLevel: OSLogType
        switch level {
        case .debug:    osLevel = .debug
        case .info:     osLevel = .info
        case .warning:  osLevel = .default
        case .error:    osLevel = .error
        }
        
        self.log(level: osLevel, tag: .sane, message)
    }
}
