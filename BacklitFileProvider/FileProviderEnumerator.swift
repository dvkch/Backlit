//
//  FileProviderEnumerator.swift
//  BacklitFileProvider
//
//  Created by syan on 25/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import FileProvider

class EmptyFileProviderEnumerator: NSObject, NSFileProviderEnumerator {
    func invalidate() { }
    func enumerateItems(for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage) {
        observer.didEnumerate([])
        observer.finishEnumerating(upTo: nil)
    }
}

class FileProviderEnumerator: NSObject, NSFileProviderEnumerator {
    
    var enumeratedItemIdentifier: NSFileProviderItemIdentifier
    
    init(enumeratedItemIdentifier: NSFileProviderItemIdentifier) {
        self.enumeratedItemIdentifier = enumeratedItemIdentifier
        super.init()
    }

    func invalidate() { }

    func enumerateItems(for observer: NSFileProviderEnumerationObserver, startingAt page: NSFileProviderPage) {
        guard let file = FileProviderItem(itemIdentifier: enumeratedItemIdentifier) else {
            return observer.finishEnumeratingWithError(NSFileProviderError(.noSuchItem))
        }

        do {
            let files = try FileManager.default.contentsOfDirectory(atPath: file.url.path)
                .compactMap { FileProviderItem(url: file.url.appendingPathComponent($0)) }
            observer.didEnumerate(files)
            observer.finishEnumerating(upTo: nil)
        }
        catch {
            return observer.finishEnumeratingWithError(error)
        }
    }
    
    func enumerateChanges(for observer: NSFileProviderChangeObserver, from anchor: NSFileProviderSyncAnchor) {
        // not mandatory and wouldn't know how to properly implement it anyway ¯\_(ツ)_/¯
    }
}
