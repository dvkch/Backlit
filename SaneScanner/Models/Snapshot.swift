//
//  Snapshot.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 26/04/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation

struct Snapshot {
    // MARK: Helpers
    static var isSnapshotting: Bool {
        ProcessInfo.processInfo.arguments.contains("DOING_SNAPSHOT")
    }
    
    // MARK: Kind
    enum Kind {
        case devicePreview, deviceOptions, deviceOptionPopup, other
    }
    
    static var kind: Kind {
        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_PREVIEW") {
            return .devicePreview
        }
        
        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_OPTIONS") {
            return .deviceOptions
        }
        
        if ProcessInfo.processInfo.arguments.contains("SNAPSHOT_OPTION_POPUP") {
            return .deviceOptionPopup
        }
        
        return .other
    }
    
    // MARK: Values
    private static func argumentValue(for name: String) -> String? {
        let prefix = name + "="
        guard let argument = ProcessInfo.processInfo.arguments.first(where: { $0.hasPrefix(prefix) }) else { return nil }
        return argument.replacingOccurrences(of: prefix, with: "")
    }

    static var snapshotTestScanImagePath: String? {
        return argumentValue(for: "SNAPSHOT_TEST_IMAGE_PATH")
    }
    
    static var snapshotHost: String? {
        return argumentValue(for: "SNAPSHOT_HOST")
    }
}

