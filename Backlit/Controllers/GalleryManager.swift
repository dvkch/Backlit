//
//  GalleryManager.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit
import SYOperationQueue
import DirectoryWatcher
import SYPictureMetadata
import SaneSwift
import LRUCache

protocol GalleryManagerDelegate: NSObjectProtocol {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem])
}

class GalleryManager: NSObject {
    
    // MARK: Init
    static let shared = GalleryManager()
    
    private override init() {
        super.init()
        
        // clear previous state
        deleteTempPDF()
        
        refreshGalleryItems()
        
        thumbsQueue.maxConcurrentOperationCount = 1
        thumbsQueue.maxSurvivingOperations = 0
        thumbsQueue.mode = .lifo
        
        watcher = DirectoryWatcher.watch(galleryFolder)
        watcher?.onNewFiles = { (files) in
            Logger.d(.gallery, "Watcher: Added \(files)")

            let unknownChangesCount = files
                .map { $0.resolvingSymlinksInPath() }
                .filter { !self.galleryItems.map(\.url).contains($0) }
                .count

            if unknownChangesCount > 0 {
                self.refreshGalleryItems()
            }
        }
        watcher?.onDeletedFiles = { (files) in
            Logger.d(.gallery, "Watcher: removed \(files)")

            // need to make sure the deleted file path starts with galleryFolder, before checking if it is indeed missing from
            // our list of items in memory; when deleting from the Files app, the deleted item path will contain symlinks, but
            // resolving them doesn't work since the file doesn't exist anymore, so we won't match the corresponding item in
            // our list of files. considering the change "unknown" if the URL either isn't resolved, or the item still is
            // in out list of files, makes sure we refresh when needed
            let unknownChangesCount = files
                .map { $0.resolvingSymlinksInPath() }
                .filter { !$0.path.hasPrefix(self.galleryFolder.path) || self.galleryItems.map(\.url).contains($0) }
                .count
            
            if unknownChangesCount > 0 {
                self.refreshGalleryItems()
            }
        }
        
        DispatchQueue.main.async {
            Snapshot.setup { config in
                config.setupGallery()
                self.refreshGalleryItems()
            }
        }
    }
    
    // MARK: Private vars
    let galleryFolder: URL = {
        // resolving symlinks because:
        // 1. the path given will be /Users/nickname/Library/Containers/me.syan.SaneScanner/Data/Pictures/Backlit/
        // 2. when we use FileManager.default.contentsOfDirectory, paths get resolved to /Users/nickname/Pictures/Backlit
        // so we need to resolve the galleryURL to make sure unicity checks using thumbsBeingCreated are working properly
        // and we don't create thumbnails twice for instance
        let url = FileManager.galleryURL.resolvingSymlinksInPath()
        try! FileManager.default.createDirectory(at: url, withIntermediateDirectories: true, attributes: [:])
        return url
    }()
    private var watcher: DirectoryWatcher?
    private var thumbsBeingCreated = [URL: [(UIImage?) -> ()]]()
    private var thumbsBeingCreatedLock = NSLock()
    private var thumbsQueue = SYOperationQueue()
    
    // MARK: Caches
    private var thumbnailCache = LRUCache<URL, UIImage>(totalCostLimit: 50_000_000, countLimit: 200)
    private var imageSizeCache = LRUCache<URL, CGSize>()
    
    // MARK: Delegates
    private class WeakDelegate: NSObject {
        typealias T = NSObject & GalleryManagerDelegate
        weak var value: T?
        init(_ value: T) {
            self.value = value
        }
        func isEqualTo(delegate: T) -> Bool {
            if let value = value {
                return value == delegate
            }
            return false
        }
        override var description: String {
            let valueDescription = value.map { "<\(type(of: $0).className): \($0.pointerAddressString)>" }
            return "<WeakDelegate: \(valueDescription ?? "<null>")>"
        }
    }
    
    private var delegates = [WeakDelegate]()

    func addDelegate(_ delegate: NSObject & GalleryManagerDelegate) {
        delegates.append(WeakDelegate(delegate))
        delegate.galleryManager(self, didUpdate: galleryItems, newItems: [], removedItems: [])
    }
    
    func removeDelegate(_ delegate: NSObject & GalleryManagerDelegate) {
        // don't use in the delegate's -dealloc method
        delegates.removeAll { (weakDelegate: WeakDelegate) in weakDelegate.isEqualTo(delegate: delegate) || weakDelegate.value == nil }
    }
    
    // MARK: Items management
    private func galleryItemForImage(at url: URL) -> GalleryItem {
        let item = GalleryItem(
            url: url,
            thumbnailUrl: thumbUrl(for: url)
        )
        return item
    }
    
    private(set) var galleryItems: [GalleryItem] = [] {
       didSet {
           handleGalleryItemsChanges(from: oldValue, to: galleryItems)
       }
    }
    private(set) var galleryGroups: [GalleryGroup] = []
    
    func createRandomTestImages(count: Int) {
        (0..<count).forEach { (_) in
            let url = galleryFolder.appendingPathComponent("testimage-\(UUID().uuidString).jpg", isDirectory: false)
            let image = UIImage.testImage(size: 500)
            try? image?.jpegData(compressionQuality: 0.9)?.write(to: url, options: .atomicWrite)
        }
    }

    func saveScans(device: Device, _ scans: [ScanImage], completion: @escaping (Result<[GalleryItem], Error>) -> ()) {
        DispatchQueue.global(qos: .userInitiated).async {
            do {
                let items = try self.saveScansSync(device: device, scans)
                DispatchQueue.main.async {
                    completion(.success(items))
                }
            }
            catch {
                DispatchQueue.main.async {
                    completion(.failure(error))
                }
            }
        }
    }

    private func saveScansSync(device: Device, _ scans: [ScanImage]) throws -> [GalleryItem] {
        Logger.d(.gallery, "Adding \(scans.count) scans...")
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        let format = Preferences.shared.imageFormat.correspondingImageFormat
        var addedItems = [GalleryItem]()

        for (index, scan) in scans.enumerated() {
            Logger.d(.gallery, "Adding scan #\(index)")
            var filename = formatter.string(from: Date())
            if scans.count > 1 {
                filename += String(format: "_%03d", index)
            }
            let fileURL = galleryFolder
                .appendingPathComponent(filename, isDirectory: false)
                .appendingPathExtension(format.fileExtension)
                .standardizedFileURL
            
            let metadata = SYMetadata.init(device: device, scanParameters: scan.parameters)
            let imageData = try scan.image.scanData(
                format: format,
                metadata: metadata.currentDictionary
            )
            
            try imageData.write(to: fileURL, options: .atomicWrite)
            let item = galleryItemForImage(at: fileURL)
            Logger.d(.gallery, "Saved image for scan #\(index) at \(fileURL)")
            
            generateThumb(for: item)
            Logger.d(.gallery, "Generated thumb for scan #\(index) at \(item.thumbnailUrl)")

            #if !targetEnvironment(macCatalyst)
            // prepare a lowres cached image if needed for when we'll be displaying the image. we do
            // it directly, to be sure it is available for the local notification
            GalleryImageView.generateLowResIfNeeded(forImageAt: fileURL)
            Logger.d(.gallery, "Generated low res for scan #\(index)")
            #endif

            // do last, as it will trigger the delegates
            galleryItems.append(item)
            addedItems.append(item)
            Logger.i(.gallery, "Finished scan #\(index)")
        }
        return addedItems
    }
    
    func deleteItem(_ item: GalleryItem) {
        Logger.i(.gallery, "Removed gallery item at #\(item.url)")
        try? FileManager.default.removeItem(at: item.url)
        try? FileManager.default.removeItem(at: item.thumbnailUrl)
        galleryItems.remove(item)
    }
    
    private class CompletionBox<T, E: Error> {
        init(completion: @escaping (Result<T, E>) -> ()) {
            self.completion = completion
        }
        var completion: (Result<T, E>) -> ()
        
        static func from(_ context: UnsafeRawPointer) -> CompletionBox<T, E> {
            return Unmanaged<CompletionBox<T, E>>
                    .fromOpaque(context)
                    .takeUnretainedValue()
        }
        var asContext: UnsafeMutableRawPointer {
            return Unmanaged.passRetained(self).toOpaque()
        }
    }
    func saveItemToCamraRoll(_ item: GalleryItem, completion: @escaping (Result<(), Error>) -> ()) {
        guard let image = UIImage(contentsOfFile: item.url.path) else {
            return completion(.failure(SaneError.noImageData))
        }
        
        UIImageWriteToSavedPhotosAlbum(
            image,
            self, #selector(self.itemSavedToCameraRoll(_:didFinishSavingWithError:contextInfo:)),
            CompletionBox(completion: completion).asContext
        )
    }

    @objc private func itemSavedToCameraRoll(_ image: UIImage, didFinishSavingWithError error: Error?, contextInfo: UnsafeRawPointer) {
        let result: Result<(), Error> = error == nil ? .success(()) : .failure(error!)
        CompletionBox<(), Error>.from(contextInfo).completion(result)
    }

    // MARK: Listing
    private func listImages() -> [GalleryItem] {
        let items = try? FileManager.default.contentsOfDirectory(
            at: galleryFolder,
            includingPropertiesForKeys: [.isDirectoryKey, .creationDateKey],
            options: .skipsSubdirectoryDescendants
        )
        
        let imageURLs = (items ?? [])
            .map { (url: URL) -> URL in url.standardizedFileURL }
            .filter { (url: URL) -> Bool in url.isSupportedImageURL && url.isDirectory == false }
            .map { galleryItemForImage(at: $0) }
            .sorted(by: \.creationDate)

        return Array(imageURLs)
    }
    
    private func refreshGalleryItems() {
        galleryItems = listImages()
    }
    
    private func handleGalleryItemsChanges(from oldItems: [GalleryItem], to newItems: [GalleryItem]) {
        guard oldItems != newItems else { return }
        galleryGroups = GalleryGroup.grouping(galleryItems)

        guard Thread.isMainThread else {
            DispatchQueue.main.async {
                self.handleGalleryItemsChanges(from: oldItems, to: newItems)
            }
            return
        }
        
        let oldItemsSet = Set(oldItems)
        let newItemsSet = Set(newItems)

        let addedItems = Array(newItemsSet.subtracting(oldItemsSet)).sorted(by: \.creationDate)
        let removedItems = Array(oldItemsSet.subtracting(newItemsSet)).sorted(by: \.creationDate)
        removedItems.forEach { (item) in
            thumbnailCache.removeValue(forKey: item.url)
            imageSizeCache.removeValue(forKey: item.url)
        }

        delegates.forEach { (weakDelegate) in
            weakDelegate.value?.galleryManager(self, didUpdate: newItems, newItems: addedItems, removedItems: removedItems)
        }
    }
    
    // MARK: Items properties
    func thumbnail(for item: GalleryItem?, completion: @escaping (UIImage?) -> ()) {
        guard let item = item else {
            return completion(nil)
        }

        if let cached = thumbnailCache.value(forKey: item.thumbnailUrl) {
            return completion(cached)
        }
        
        DispatchQueue.global(qos: .background).async {
            self.generateThumb(for: item) { thumb in
                self.thumbnailCache.setValue(thumb, forKey: item.thumbnailUrl, cost: thumb?.estimatedMemoryFootprint ?? 0)
                DispatchQueue.main.async {
                    completion(thumb)
                }
            }
        }
    }

    func imageSize(for item: GalleryItem) -> CGSize? {
        if let size = self.imageSizeCache.value(forKey: item.url) {
            return size
        }
        
        guard let imageSize = UIImage.sizeOfImage(at: item.url) else { return nil }

        imageSizeCache.setValue(imageSize, forKey: item.url)
        return imageSize
    }

    // MARK: Thumb
    private func generateThumb(for item: GalleryItem, completion: ((UIImage?) -> ())? = nil) {
        // if thumb already exists, we return it
        guard !FileManager.default.fileExists(atPath: item.thumbnailUrl.path) else {
            completion?(UIImage(contentsOfFile: item.thumbnailUrl.path))
            return
        }

        // if we are already creating the thumb, we add our callback to the list
        thumbsBeingCreatedLock.lock()
        if let pendingCallbacks = thumbsBeingCreated[item.url] {
            thumbsBeingCreated[item.url] = pendingCallbacks + [completion].removingNils()
            thumbsBeingCreatedLock.unlock()
            return
        }
        
        // start the task
        thumbsBeingCreated[item.url] = [completion].removingNils()
        thumbsBeingCreatedLock.unlock()

        // preferred way of doing resizes, as it doesn't use a lot of memory on device
        let thumb = UIImage.thumbnailForImage(at: item.url, maxEdgeSize: 200, options: .alwaysCreate)
        try? thumb?
            .jpegData(compressionQuality: 0.6)?
            .write(to: item.thumbnailUrl, options: .atomicWrite)
        
        // cache
        thumbnailCache.setValue(thumb, forKey: item.url, cost: thumb?.estimatedMemoryFootprint ?? 0)

        // send callbacks
        thumbsBeingCreatedLock.lock()
        let callbacks = self.thumbsBeingCreated[item.url] ?? []
        self.thumbsBeingCreated[item.url] = nil
        thumbsBeingCreatedLock.unlock()

        DispatchQueue.main.async {
            callbacks.forEach { $0(thumb) }
        }
    }
    
    // MARK: PDF
    func tempPdfFileUrl() -> URL {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        let filename = "Backlit_\(formatter.string(from: Date())).pdf"
        return pdfFolderURL.appendingPathComponent(filename, isDirectory: false)
    }
    
    func deleteTempPDF() {
        FileManager.default.emptyCacheDirectory(.pdfGeneration)
    }

    // MARK: Paths
    private var thumbnailsFolderURL: URL {
        return FileManager.cacheDirectory(.thumbnails)
    }
    
    private var pdfFolderURL: URL {
        return FileManager.cacheDirectory(.pdfGeneration)
    }
    
    private func thumbUrl(for imageUrl: URL) -> URL {
        let filename = imageUrl.deletingPathExtension().appendingPathExtension("thumbs.jpg").lastPathComponent
        return thumbnailsFolderURL.appendingPathComponent(filename, isDirectory: false)
    }
}

private extension URL {
    var isSupportedImageURL: Bool {
        return ["jpg", "png", "heic"].contains(pathExtension.lowercased())
    }
}
