//
//  FileManager+SYKit.swift
//  Pods-SYKitExample
//
//  Created by Stanislas Chevallier on 27/06/2019.
//

import Foundation

public extension FileManager {
    @available(tvOS, unavailable)
    @objc(sy_documentsPath)
    var documentsPath: URL {
        return urls(for: .documentDirectory, in: .userDomainMask).last!
    }
    
    @objc(sy_cachePath)
    var cachePath: URL {
        return urls(for: .cachesDirectory, in: .userDomainMask).last!
    }
}

public extension URL {
    @available(tvOS, unavailable)
    init(fileURLWithDocsPath path: String) {
        self.init(fileURLWithPath: FileManager.default.documentsPath.appendingPathComponent(path).path)
    }
    
    init(fileURLWithCachePath path: String) {
        self.init(fileURLWithPath: FileManager.default.cachePath.appendingPathComponent(path).path)
    }
}
