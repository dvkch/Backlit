//
//  GalleryManager.swift
//  SaneScanner
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
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem)
}

class GalleryManager: NSObject {
    
    // MARK: Init
    static let shared = GalleryManager()
    
    private override init() {
        super.init()
        
        // clear previous state
        deleteTempPDF()
        
        refreshImageList()
        
        thumbsQueue.maxConcurrentOperationCount = 1
        thumbsQueue.maxSurvivingOperations = 0
        thumbsQueue.mode = .LIFO
        
        watcher = DirectoryWatcher.watch(galleryFolder)
        watcher?.onNewFiles = { (files) in
            let unknownChangesCount = files.filter { !self.imageURLs.contains($0.standardizedFileURL) }.count
            if unknownChangesCount > 0 {
                self.refreshImageList()
            }
        }
        watcher?.onDeletedFiles = { (files) in
            let unknownChangesCount = files.filter { self.imageURLs.contains($0.standardizedFileURL) }.count
            if unknownChangesCount > 0 {
                self.refreshImageList()
            }
        }
    }
    
    // MARK: Private vars
    let galleryFolder: URL = {
        // resolving symlinks because:
        // 1. the path given will be /Users/nickname/Library/Containers/me.syan.SaneScanner/Data/Pictures/SaneScanner/
        // 2. when we use FileManager.default.contentsOfDirectory, paths get resolved to /Users/nickname/Pictures/SaneScanner
        // so we need to resolve the galleryURL to make sure unicity checks using thumbsBeingCreated are working properly
        // and we don't create thumbnails twice for instance
        let url = FileManager.galleryURL.resolvingSymlinksInPath()
        try! FileManager.default.createDirectory(at: url, withIntermediateDirectories: true, attributes: [:])
        return url
    }()
    private var watcher: DirectoryWatcher?
    private var thumbsBeingCreated = [URL]()
    private var thumbsQueue = SYOperationQueue()
    
    // MARK: Caches
    private var thumbnailCache = LRUCache<URL, UIImage>(totalCostLimit: 50_000_000, countLimit: 100)
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
            let valueDescription = value.map { "<\(type(of: $0).className): \(Unmanaged.passUnretained($0 as NSObject).toOpaque())>" }
            return "<WeakDelegate: \(valueDescription ?? "<null>")>"
        }
    }
    
    private var delegates = [WeakDelegate]()

    func addDelegate(_ delegate: NSObject & GalleryManagerDelegate) {
        delegates.append(WeakDelegate(delegate))
        delegate.galleryManager(self, didUpdate: items, newItems: [], removedItems: [])
    }
    
    func removeDelegate(_ delegate: NSObject & GalleryManagerDelegate) {
        // don't use in the delegate's -dealloc method
        delegates.removeAll { (weakDelegate: WeakDelegate) in weakDelegate.isEqualTo(delegate: delegate) || weakDelegate.value == nil }
    }
    
    // MARK: Items management
    private var imageURLs = [URL]() {
        didSet {
            handleImageURLsChanges(from: oldValue, to: imageURLs)
        }
    }
    
    private func galleryItemForImage(at url: URL) -> GalleryItem {
        let item = GalleryItem(
            url: url,
            thumbnailUrl: thumbUrl(for: url)
        )
        return item
    }
    
    var items: [GalleryItem] {
        return imageURLs.map { galleryItemForImage(at: $0) }
    }
    
    func createRandomTestImages(count: Int) {
        (0..<count).forEach { (_) in
            let url = galleryFolder.appendingPathComponent("testimage-\(UUID().uuidString).jpg", isDirectory: false)
            let image = UIImage.testImage(size: 500)
            try? image?.jpegData(compressionQuality: 0.9)?.write(to: url, options: .atomicWrite)
        }
    }
    
    func saveScans(device: Device, _ scans: [ScanImage]) throws {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        let format = Preferences.shared.imageFormat.correspondingImageFormat
        for (index, scan) in scans.enumerated() {
            var filename = formatter.string(from: Date())
            if scans.count > 1 {
                filename += String(format: "_%03d", index)
            }
            let fileURL = galleryFolder
                .appendingPathComponent(filename, isDirectory: false)
                .appendingPathExtension(format.fileExtension)
            
            let metadata = SYMetadata.init(device: device, scanParameters: scan.parameters)
            let imageData = scan.image.scanData(
                format: format,
                metadata: metadata.currentDictionary
            )
            guard let imageData else { throw SaneError.cannotGenerateImage }
            
            try imageData.write(to: fileURL, options: .atomicWrite)
            let item = galleryItemForImage(at: fileURL.standardizedFileURL)
            generateThumb(for: item, async: false, tellDelegates: true)
            
            #if !targetEnvironment(macCatalyst)
            // prepare a lowres cached image if needed for when we'll be displaying the image
            DispatchQueue.global(qos: .background).async {
                GalleryImageView.generateLowResIfNeeded(forImageAt: fileURL)
            }
            #endif

            // do last, as it will trigger the delegates
            imageURLs.insert(fileURL.standardizedFileURL, at: 0)
        }
    }
    
    func deleteItem(_ item: GalleryItem) {
        try? FileManager.default.removeItem(at: item.url)
        try? FileManager.default.removeItem(at: item.thumbnailUrl)
        
        if let firstIndex = imageURLs.firstIndex(of: item.url) {
            imageURLs.remove(at: firstIndex)
        }
    }

    private func listImages() -> [URL] {
        let items = try? FileManager.default.contentsOfDirectory(
            at: galleryFolder,
            includingPropertiesForKeys: [.isDirectoryKey, .creationDateKey],
            options: .skipsSubdirectoryDescendants
        )
        
        let imageURLs = (items ?? [])
            .map { (url: URL) -> URL in url.standardizedFileURL }
            .filter { (url: URL) -> Bool in url.isSupportedImageURL && url.isDirectory == false && url.creationDate != nil }
            .sorted { $0.creationDate! > $1.creationDate! }

        return imageURLs
    }
    
    private func refreshImageList() {
        imageURLs = listImages()
    }
    
    private func handleImageURLsChanges(from oldURLs: [URL], to newURLs: [URL]) {
        if oldURLs == newURLs { return }
        
        guard Thread.isMainThread else {
            DispatchQueue.main.async {
                self.handleImageURLsChanges(from: oldURLs, to: newURLs)
            }
            return
        }
        
        let oldURLsSet = Set(oldURLs)
        let newURLsSet = Set(newURLs)

        let addedItems = newURLsSet
            .subtracting(oldURLs)
            .map { galleryItemForImage(at: $0) }
        
        let removedItems = oldURLsSet
            .subtracting(newURLsSet)
            .map { galleryItemForImage(at: $0) }

        removedItems.forEach { (item) in
            thumbnailCache.removeValue(forKey: item.url)
            imageSizeCache.removeValue(forKey: item.url)
        }
        
        let items = self.items
        delegates.forEach { (weakDelegate) in
            weakDelegate.value?.galleryManager(self, didUpdate: items, newItems: addedItems, removedItems: removedItems)
        }
    }
    
    // MARK: Items properties
    func thumbnail(for item: GalleryItem?) -> UIImage? {
        guard let item = item else { return nil }
        if let cached = thumbnailCache.value(forKey: item.thumbnailUrl) {
            return cached
        }
        if let fromFile = UIImage(contentsOfFile: item.thumbnailUrl.path) {
            thumbnailCache.setValue(fromFile, forKey: item.thumbnailUrl, cost: fromFile.estimatedMemoryFootprint)
            return fromFile
        }
        generateThumb(for: item, async: true, tellDelegates: true)
        return nil
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
    private func generateThumb(for item: GalleryItem, async: Bool, tellDelegates: Bool) {
        if async {
            thumbsQueue.addOperation {
                self.generateThumb(for: item, async: false, tellDelegates: tellDelegates)
            }
            return
        }
        
        guard !thumbsBeingCreated.contains(item.url) else { return }
        thumbsBeingCreated.append(item.url)

        let dequeue = { [weak self] () -> () in
            self?.thumbsBeingCreated.removeAll { $0 == item.url }
        }

        // preferred way of doing resizes, as it doesn't use a lot of memory on device
        let thumb = UIImage.thumbnailForImage(at: item.url, maxEdgeSize: 200, options: .alwaysCreate)
        guard let thumb else { return dequeue() }

        try? thumb
            .jpegData(compressionQuality: 0.6)?
            .write(to: item.thumbnailUrl, options: .atomicWrite)
        
        self.thumbnailCache.setValue(thumb, forKey: item.url, cost: thumb.estimatedMemoryFootprint)
        dequeue()
        
        guard tellDelegates else { return }
        DispatchQueue.main.async {
            self.delegates.forEach { (weakDelegate) in
                weakDelegate.value?.galleryManager(self, didCreate: thumb, for: item)
            }
        }
    }
    
    // MARK: PDF
    func tempPdfFileUrl() -> URL {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        let filename = "SaneScanner_\(formatter.string(from: Date())).pdf"
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
