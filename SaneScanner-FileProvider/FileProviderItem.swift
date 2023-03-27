//
//  FileProviderItem.swift
//  SaneScanner-FileProvider
//
//  Created by Stanislas Chevallier on 25/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import FileProvider
import MobileCoreServices

extension URL {
    func path(relativeTo otherURL: URL) -> String? {
        guard pathComponents.count >= otherURL.pathComponents.count else { return nil }
        guard Array(pathComponents[0..<otherURL.pathComponents.count]) == otherURL.pathComponents else { return nil }
        return pathComponents.dropFirst(otherURL.pathComponents.count).joined(separator: "/")
    }
}

class FileProviderItem: NSObject {
    // MARK: Init
    init(path: String) {
        self.path = path
    }

    convenience init?(url: URL) {
        guard let path = url.path(relativeTo: FileManager.galleryURL) else { return nil }
        self.init(path: path)
    }

    convenience init?(itemIdentifier: NSFileProviderItemIdentifier) {
        if itemIdentifier == .rootContainer {
            self.init(path: "")
        }
        else {
            self.init(path: itemIdentifier.rawValue)
        }
    }

    // MARK: Properties
    private let path: String
    var url: URL {
        FileManager.galleryURL.appendingPathComponent(path)
    }
    var isRoot: Bool {
        return path.isEmpty
    }
    var isDirectory: Bool {
        return (try? url.resourceValues(forKeys: Set([.isDirectoryKey])).isDirectory) == true
    }
    override var description: String {
        return "<FileProviderItem path=\(path), isRoot=\(isRoot)>"
    }
    
    // MARK: Actions
    func rename(to filename: String) throws -> FileProviderItem {
        let newURL = url.deletingLastPathComponent().appendingPathComponent(filename)
        guard let newItem = FileProviderItem(url: newURL) else {
            throw NSFileProviderError(.noSuchItem)
        }

        guard !FileManager.default.fileExists(atPath: newURL.absoluteURL.path) else {
            throw NSFileProviderError(.filenameCollision)
        }

        newItem.tagData = tagData
        tagData = nil
        try FileManager.default.moveItem(at: url, to: newURL)
        return newItem
    }
    
    func delete() throws {
        tagData = nil
        try FileManager.default.removeItem(at: url)
    }
}

extension FileProviderItem: NSFileProviderItem {
    var itemIdentifier: NSFileProviderItemIdentifier {
        if isRoot {
            return .rootContainer
        }
        return NSFileProviderItemIdentifier(path)
    }
    
    var parentItemIdentifier: NSFileProviderItemIdentifier {
        return FileProviderItem(path: (path as NSString).deletingLastPathComponent).itemIdentifier
    }
    
    var capabilities: NSFileProviderItemCapabilities {
        if isRoot {
            return [.allowsContentEnumerating, .allowsReading, .allowsAddingSubItems]
        }
        return [.allowsContentEnumerating, .allowsReading, .allowsAddingSubItems, .allowsRenaming, .allowsDeleting]
    }
    
    var filename: String {
        if isRoot {
            return "SaneScanner"
        }
        return (path as NSString).lastPathComponent
    }
    
    var creationDate: Date? {
        return try? url.resourceValues(forKeys: Set([URLResourceKey.creationDateKey])).creationDate
    }
    
    var contentModificationDate: Date? {
        return try? url.resourceValues(forKeys: Set([URLResourceKey.contentModificationDateKey])).contentModificationDate
    }
    
    var lastUsedDate: Date? {
        return try? url.resourceValues(forKeys: Set([URLResourceKey.contentAccessDateKey])).contentAccessDate
    }
    
    var documentSize: NSNumber? {
        guard !isDirectory else { return nil }
        return try? url.resourceValues(forKeys: Set([URLResourceKey.fileSizeKey])).fileSize as NSNumber?
    }
    
    var typeIdentifier: String {
        guard !isDirectory else { return kUTTypeFolder as String }
        return (try? url.resourceValues(forKeys: Set([.typeIdentifierKey])).typeIdentifier) ?? (kUTTypeItem as String)
    }
    
    var tagData: Data? {
        get { UserDefaults(suiteName: "group.me.syan.SaneScanner")?.data(forKey: path) }
        set { UserDefaults(suiteName: "group.me.syan.SaneScanner")?.set((newValue?.isEmpty ?? true) ? nil : newValue, forKey: path) }
    }
}
