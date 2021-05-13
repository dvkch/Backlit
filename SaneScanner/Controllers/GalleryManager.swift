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

protocol GalleryManagerDelegate: NSObjectProtocol {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem])
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem)
}

private let kImageExtensionPNG  = "png"
private let kImageExtensionJPG  = "jpg"
private let kImageThumbsSuffix  = "thumbs.jpg"
private let kImageThumbsFolder  = "thumbs"
private let kImagePDFFolder     = "PDF"
private let kImagePDFPrefix     = "SaneScanner_"
private let kImageExtensionPDF  = "pdf"

class GalleryManager: NSObject {
    
    // MARK: Init
    static let shared = GalleryManager()
    
    private override init() {
        super.init()
        
        // create folders if needed
        try? FileManager.default.createDirectory(at: thumbnailsFolderURL, withIntermediateDirectories: true, attributes: nil)
        try? FileManager.default.createDirectory(at: pdfFolderURL, withIntermediateDirectories: true, attributes: nil)

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
        
        NotificationCenter.default.addObserver(self, selector: #selector(self.receivedMemoryWarning), name: UIApplication.didReceiveMemoryWarningNotification, object: nil)
    }
    
    // MARK: Private vars
    let galleryFolder: URL = {
        #if targetEnvironment(macCatalyst)
        let url = FileManager.imageDirectoryURL.appendingPathComponent("SaneScanner", isDirectory: true)
        try! FileManager.default.createDirectory(at: url, withIntermediateDirectories: true, attributes: [:])
        return url
        #else
        return FileManager.documentsDirectoryURL
        #endif
    }()
    private var watcher: DirectoryWatcher?
    private var thumbsBeingCreated = [URL]()
    private var thumbsQueue = SYOperationQueue()
    
    // MARK: Caches
    private var thumbnailCache = NSCache<NSURL, UIImage>()
    private var imageSizeCache = NSCache<NSURL, NSValue>()
    
    @objc private func receivedMemoryWarning() {
        // recreate instead of clearing, because after a memory warning an NSCache instance could be broken
        thumbnailCache = NSCache<NSURL, UIImage>()
        imageSizeCache = NSCache<NSURL, NSValue>()
    }
    
    // MARK: Delegates
    private class WeakDelegate {
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
    
    @discardableResult func addImage(_ image: UIImage, metadata: SYMetadata?) throws -> GalleryItem? {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        var fileURL = galleryFolder.appendingPathComponent(formatter.string(from: Date()), isDirectory: false)
        
        let imageData: Data
        if Preferences.shared.saveAsPNG {
            fileURL.appendPathExtension(kImageExtensionPNG)
            if let data = image.pngData() {
                imageData = data
            } else {
                return nil
            }
        } else {
            fileURL.appendPathExtension(kImageExtensionJPG)
            if let data = image.jpegData(compressionQuality: 0.9) {
                imageData = data
            } else {
                return nil
            }
        }
        
        let imageDataWithMetadata = try metadata?.apply(to: imageData)

        let dataToWrite = imageDataWithMetadata ?? imageData
        try dataToWrite.write(to: fileURL, options: .atomicWrite)
        imageURLs.insert(fileURL.standardizedFileURL, at: 0)
        
        let item = galleryItemForImage(at: fileURL)
        generateThumbAsync(for: item, fullImage: image, tellDelegates: true)
        return item
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
        
        let addedItems = newURLs
            .filter { !oldURLs.contains($0) }
            .map { galleryItemForImage(at: $0) }
        
        let removedItems = oldURLs
            .filter { !newURLs.contains($0) }
            .map { galleryItemForImage(at: $0) }

        removedItems.forEach { (item) in
            thumbnailCache.removeObject(forKey: item.url as NSURL)
            imageSizeCache.removeObject(forKey: item.url as NSURL)
        }
        
        let items = self.items
        delegates.forEach { (weakDelegate) in
            weakDelegate.value?.galleryManager(self, didUpdate: items, newItems: addedItems, removedItems: removedItems)
        }
    }
    
    // MARK: Items properties
    func thumbnail(for item: GalleryItem?) -> UIImage? {
        guard let item = item else { return nil }
        
        let image = thumbnailCache.object(forKey: item.thumbnailUrl as NSURL) ?? UIImage(contentsOfFile: item.thumbnailUrl.path)
        if image == nil {
            generateThumbAsync(for: item, fullImage: nil, tellDelegates: true)
        }
        return image
    }
    
    func dateString(for item: GalleryItem) -> String? {
        guard let date = item.url.creationDate else { return nil }

        let formatter = DateFormatter()
        formatter.dateStyle = .long
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
    
    func imageSize(for item: GalleryItem) -> CGSize? {
        if let size = self.imageSizeCache.object(forKey: item.url as NSURL)?.cgSizeValue {
            return size
        }
        
        guard let imageSize = UIImage.sizeOfImage(at: item.url) else { return nil }

        imageSizeCache.setObject(NSValue(cgSize: imageSize), forKey: item.url as NSURL)
        return imageSize
    }
    
    // MARK: Thumb
    private func generateThumbAsync(for item: GalleryItem, fullImage: UIImage?, tellDelegates: Bool) {
        guard !thumbsBeingCreated.contains(item.url) else { return }
        thumbsBeingCreated.append(item.url)
        
        let dequeue = { [weak self] in
            if let index = self?.thumbsBeingCreated.firstIndex(of: item.url) {
                self?.thumbsBeingCreated.remove(at: index)
            }
        }

        thumbsQueue.addOperation {
            // this first method is a bit longer to generate images, but uses far less memory on the device
            var thumb = UIImage.thumbnailForImage(at: item.url, maxEdgeSize: 200)
            
            // in case the first method fails we do it the old way
            if thumb == nil {
                guard let original = fullImage ?? UIImage(contentsOfFile: item.url.path) else { return dequeue() }
                thumb = original.resizingLongestEdge(to: 200)
            }
            
            guard thumb != nil else { return dequeue() }
            
            try? thumb!
                .jpegData(compressionQuality: 0.6)?
                .write(to: item.thumbnailUrl, options: .atomicWrite)
            
            self.thumbnailCache.setObject(thumb!, forKey: item.url as NSURL)
            
            dequeue()
            
            guard tellDelegates else { return }
            
            DispatchQueue.main.async {
                self.delegates.forEach { (weakDelegate) in
                    weakDelegate.value?.galleryManager(self, didCreate: thumb!, for: item)
                }
            }
        }
    }
    
    
    // MARK: PDF
    func tempPdfFileUrl() -> URL {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        let filename = kImagePDFPrefix + formatter.string(from: Date())
        return pdfFolderURL.appendingPathComponent(filename, isDirectory: false).appendingPathExtension(kImageExtensionPDF)
    }
    
    func deleteTempPDF() {
        // remove pdf files in cache / PDF
        let tempURLs = try? FileManager.default.contentsOfDirectory(at: pdfFolderURL, includingPropertiesForKeys: nil, options: .skipsSubdirectoryDescendants)
        
        tempURLs?.forEach { (url) in
            try? FileManager.default.removeItem(at: url)
        }
    }

    // MARK: Paths
    private var thumbnailsFolderURL: URL {
        return FileManager.cacheDirectoryURL.appendingPathComponent(kImageThumbsFolder, isDirectory: true)
    }
    
    private var pdfFolderURL: URL {
        return FileManager.cacheDirectoryURL.appendingPathComponent(kImagePDFFolder, isDirectory: true)
    }
    
    private func thumbUrl(for imageUrl: URL) -> URL {
        let filename = imageUrl.deletingPathExtension().appendingPathExtension(kImageThumbsSuffix).lastPathComponent
        return thumbnailsFolderURL.appendingPathComponent(filename, isDirectory: false)
    }
}

private extension URL {
    var isSupportedImageURL: Bool {
        return [kImageExtensionPNG.lowercased(), kImageExtensionJPG.lowercased()].contains(pathExtension.lowercased())
    }
}
