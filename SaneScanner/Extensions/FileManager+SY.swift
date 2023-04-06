//
//  FileManager+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit


// MARK: Scans storage
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
}

// MARK: Cache storage
extension FileManager {
    private static var cacheDirectoryURL: URL {
        return try! FileManager.default.url(for: .cachesDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
    }

    enum CacheDirectory: String, CaseIterable {
        case thumbnails     = "thumbs"
        case lowRes         = "lowres"
        case pdfGeneration  = "PDF"
    }

    static func cacheDirectory(_ cache: CacheDirectory) -> URL {
        let url = cacheDirectoryURL.appendingPathComponent(cache.rawValue, isDirectory: true)
        try? FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }
    
    func emptyCacheDirectory(_ cache: CacheDirectory) {
        try? emptyDirectory(at: FileManager.cacheDirectory(cache))
    }
    
    func emptyCacheDirectories() {
        CacheDirectory.allCases.forEach { dir in
            emptyCacheDirectory(dir)
        }
    }
}

// MARK: Helpers
extension FileManager {
    private func emptyDirectory(at url: URL) throws {
        try contentsOfDirectory(
            at: url,
            includingPropertiesForKeys: [URLResourceKey.isDirectoryKey],
            options: .skipsSubdirectoryDescendants
        ).forEach { item in
            if try item.resourceValues(forKeys: Set([.isDirectoryKey])).isDirectory == true {
                try emptyDirectory(at: item)
            }
            try removeItem(at: item)
        }
    }
}
