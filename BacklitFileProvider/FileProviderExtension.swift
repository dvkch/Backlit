//
//  FileProviderExtension.swift
//  BacklitFileProvider
//
//  Created by Stanislas Chevallier on 25/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import FileProvider

class FileProviderExtension: NSFileProviderExtension {
    override func item(for identifier: NSFileProviderItemIdentifier) throws -> NSFileProviderItem {
        // resolve the given identifier to a record in the model
        guard let item = FileProviderItem(itemIdentifier: identifier) else {
            throw NSFileProviderError(.noSuchItem)
        }
        return item
    }
    
    override func urlForItem(withPersistentIdentifier identifier: NSFileProviderItemIdentifier) -> URL? {
        // resolve the given identifier to a file on disk
        guard let item = try? item(for: identifier) as? FileProviderItem else {
            return nil
        }
        
        // in this implementation, all paths are structured as <base storage directory>/<item identifier>/<item file name>
        return item.url
    }
    
    override func persistentIdentifierForItem(at url: URL) -> NSFileProviderItemIdentifier? {
        // resolve the given URL to a persistent identifier using a database
        return FileProviderItem(url: url)?.itemIdentifier
    }
    
    override func providePlaceholder(at url: URL, completionHandler: @escaping (Error?) -> Void) {
        // we don't allow adding files
        completionHandler(NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:]))
    }

    override func startProvidingItem(at url: URL, completionHandler: @escaping ((_ error: Error?) -> Void)) {
        completionHandler(nil)
    }
    
    override func itemChanged(at url: URL) {
        // not needed
    }
    
    override func stopProvidingItem(at url: URL) {
        // not needed
    }
    
    // MARK: - Actions
    override func setTagData(_ tagData: Data?, forItemIdentifier itemIdentifier: NSFileProviderItemIdentifier, completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void) {
        guard let item = FileProviderItem(itemIdentifier: itemIdentifier) else {
            return completionHandler(nil, NSFileProviderError(.noSuchItem))
        }

        item.tagData = tagData
        completionHandler(item, nil)
    }
    
    override func importDocument(at fileURL: URL, toParentItemIdentifier parentItemIdentifier: NSFileProviderItemIdentifier, completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void) {
        guard let parentItem = FileProviderItem(itemIdentifier: parentItemIdentifier) else {
            completionHandler(nil, NSError.fileProviderErrorForNonExistentItem(withIdentifier: parentItemIdentifier))
            return
        }

        do {
            let newFileURL = parentItem.url.appendingPathComponent(fileURL.lastPathComponent, isDirectory: false)
            try FileManager.default.moveItem(at: fileURL, to: newFileURL)
            completionHandler(FileProviderItem(url: newFileURL), nil)
        }
        catch {
            completionHandler(nil, error)
        }
    }
    
    override func renameItem(withIdentifier itemIdentifier: NSFileProviderItemIdentifier, toName itemName: String, completionHandler: @escaping (NSFileProviderItem?, Error?) -> Void) {
        guard let item = FileProviderItem(itemIdentifier: itemIdentifier) else {
            return completionHandler(nil, NSFileProviderError(.noSuchItem))
        }

        do {
            let newItem = try item.rename(to: itemName)
            completionHandler(newItem, nil)
        }
        catch {
            completionHandler(nil, error)
        }
    }
    
    override func deleteItem(withIdentifier itemIdentifier: NSFileProviderItemIdentifier, completionHandler: @escaping (Error?) -> Void) {
        guard let item = FileProviderItem(itemIdentifier: itemIdentifier) else {
            return completionHandler(NSFileProviderError(.noSuchItem))
        }

        do {
            try item.delete()
            completionHandler(nil)
        }
        catch {
            completionHandler(error)
        }
    }
    
    // MARK: - Enumeration
    override func enumerator(for containerItemIdentifier: NSFileProviderItemIdentifier) throws -> NSFileProviderEnumerator {
        if containerItemIdentifier == .workingSet {
            return EmptyFileProviderEnumerator()
        }

        guard let file = FileProviderItem(itemIdentifier: containerItemIdentifier) else {
            throw NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:])
        }

        if file.isDirectory {
            return FileProviderEnumerator(enumeratedItemIdentifier: containerItemIdentifier)
        }
        
        throw NSError(domain: NSCocoaErrorDomain, code: NSFeatureUnsupportedError, userInfo:[:])
    }
}
