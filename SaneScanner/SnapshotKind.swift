//
//  SnapshotKind.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 26/04/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation

enum SnapshotKind {
    case none, devicePreview, deviceOptions, deviceOptionPopup, other
    
    static var fromLaunchOptions: Self {
        if ProcessInfo.processInfo.arguments.contains("DOING_SNAPSHOT") {
            return .other
        }

        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_PREVIEW") {
            return .devicePreview
        }
        
        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_OPTIONS") {
            return .deviceOptions
        }
        
        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_OPTION_POPUP") {
            return .deviceOptionPopup
        }
        
        return .none
    }
    
    static var snapshotTestScanImagePath: String? {
        let testPathPrefix = "SNAPSHOT_TEST_IMAGE_PATH="
        if let testPathArgument = ProcessInfo.processInfo.arguments.first(where: { $0.hasPrefix(testPathPrefix) }) {
            return testPathArgument.replacingOccurrences(of: testPathPrefix, with: "")
        }
        return nil
    }
}

