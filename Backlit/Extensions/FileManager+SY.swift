//
//  FileManager+SY.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

// MARK: Scans storage
extension FileManager {
    static var galleryURL: URL {
        #if targetEnvironment(macCatalyst)
        FileManager.imageDirectoryURL.appendingPathComponent("Backlit", isDirectory: true)
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
    
    func sizeOfCacheDirectory(_ cache: CacheDirectory, completion: @escaping (Int64?) -> ()) {
        DispatchQueue.global(qos: .utility).async {
            let size: Int64?
            do {
                size = try self.sizeOfItem(at: FileManager.cacheDirectory(cache))
            }
            catch {
                Logger.e(.app, "Couldn't estimate size of \(cache.rawValue) cache: \(error)")
                size = nil
            }
            DispatchQueue.main.async {
                completion(size)
            }
        }
    }
    
    func sizeOfCacheDirectories(completion: @escaping (Int64) -> ()) {
        DispatchQueue.global(qos: .utility).async {
            var size: Int64 = 0
            CacheDirectory.allCases.forEach { dir in
                do {
                    size += try self.sizeOfItem(at: FileManager.cacheDirectory(dir))
                }
                catch {
                    Logger.e(.app, "Couldn't estimate size of \(dir.rawValue) cache: \(error)")
                }
            }
            DispatchQueue.main.async {
                completion(size)
            }
        }
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
    
    private func sizeOfItem(at url: URL) throws -> Int64 {
        guard url.hasDirectoryPath else {
            return Int64((try? url.allocatedFileSize) ?? 0)
        }
        
        var size: Int64 = 0
        try contentsOfDirectory(
            at: url,
            includingPropertiesForKeys: [URLResourceKey.isDirectoryKey, .totalFileAllocatedSizeKey, .fileAllocatedSizeKey],
            options: .skipsSubdirectoryDescendants
        ).forEach { item in
            if try item.resourceValues(forKeys: Set([.isDirectoryKey])).isDirectory == true {
                size += try sizeOfItem(at: item)
            }
            else {
                size += Int64((try? item.allocatedFileSize) ?? 0)
            }
        }
        
        return size
    }
}

extension URL {
    fileprivate var allocatedFileSize: Int {
        get throws {
            let values = try resourceValues(forKeys: Set([URLResourceKey.totalFileAllocatedSizeKey, .fileAllocatedSizeKey]))
            return values.totalFileAllocatedSize ?? values.fileAllocatedSize ?? 0
        }
    }
}
