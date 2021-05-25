//
//  FileManager+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension FileManager {
    static var galleryURL: URL {
        #if targetEnvironment(macCatalyst)
        FileManager.imageDirectoryURL.appendingPathComponent("SaneScanner", isDirectory: true)
        #else
        FileManager.sharedGroupDirectoryURL.appendingPathComponent("Scans", isDirectory: true)
        #endif
    }
    
    private static var sharedGroupDirectoryURL: URL {
        return FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.me.syan.SaneScanner")!
    }
    
    private static var imageDirectoryURL: URL {
        return try! FileManager.default.url(for: .picturesDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
    }
    
    static var cacheDirectoryURL: URL {
        return try! FileManager.default.url(for: .cachesDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
    }
    
    static var appSupportDirectoryURL: URL {
        let url = try! FileManager.default.url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
            .appendingPathComponent("SaneScanner", isDirectory: true)
        
        if !FileManager.default.fileExists(atPath: url.path) {
            try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true, attributes: nil)
        }
        
        return url
    }
}

