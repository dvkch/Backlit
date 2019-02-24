//
//  GalleryManager.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYPictureMetadata
import SYKit
import SYOperationQueue
import DirectoryWatcher

protocol GalleryManagerDelegate: AnyObject, NSObjectProtocol {
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
        
        watcher = DirectoryWatcher.watch(FileManager.documentsDirectoryURL)
        // TODO: don't pull all data again since we know what changed
        watcher?.onNewFiles = { [weak self] _ in
            self?.refreshImageList()
        }
        watcher?.onDeletedFiles = watcher?.onNewFiles
        
        NotificationCenter.default.addObserver(self, selector: #selector(self.receivedMemoryWarning), name: UIApplication.didReceiveMemoryWarningNotification, object: nil)
    }
    
    // MARK: Private vars
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
    private var imageURLs = [URL]()
    
    private func galleryItemForImage(at url: URL) -> GalleryItem {
        let item = GalleryItem(
            URL: url,
            thumbnailURL: thumbURL(for: url)
        )
        return item
    }
    
    var items: [GalleryItem] {
        return imageURLs.map { galleryItemForImage(at: $0) }
    }
    
    func createRandomTestImages(count: Int) {
        (0..<count).forEach { (_) in
            let url = FileManager.documentsDirectoryURL.appendingPathComponent("testimage-\(UUID().uuidString).jpg", isDirectory: false)
            let image = UIImage.imageWithColor(.random, size: CGSize(width: 10, height: 10), cornerRadius: 0)
            try? image?.jpegData(compressionQuality: 0.9)?.write(to: url, options: .atomicWrite)
        }
    }
    
    @discardableResult func addImage(_ image: UIImage, metadata: SYMetadata?) -> GalleryItem? {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        var fileURL = FileManager.documentsDirectoryURL.appendingPathComponent(formatter.string(from: Date()), isDirectory: false)
        
        let imageData: Data?
        if Preferences.shared.saveAsPNG {
            fileURL.appendPathExtension(kImageExtensionPNG)
            imageData = image.pngData()
        } else {
            fileURL.appendPathExtension(kImageExtensionJPG)
            imageData = image.jpegData(compressionQuality: 0.9)
        }
        
        guard imageData != nil else { return nil }
        
        var imageDataWithMetadata = imageData
        if let metadata = metadata {
            imageDataWithMetadata = SYMetadata.data(withImageData: imageData, andMetadata: metadata)
        }
        
        guard let dataToWrite = imageDataWithMetadata ?? imageData else { return nil }

        try? dataToWrite.write(to: fileURL, options: .atomicWrite)
        let item = galleryItemForImage(at: fileURL)
        generateThumbAsync(for: item, fullImage: image, tellDelegates: true)
        return item
    }
    
    func deleteItem(_ item: GalleryItem) {
        try? FileManager.default.removeItem(at: item.URL)
        try? FileManager.default.removeItem(at: item.thumbnailURL)
    }

    private func listImages() -> [URL] {
        let items = try? FileManager.default.contentsOfDirectory(
            at: FileManager.documentsDirectoryURL,
            includingPropertiesForKeys: [.isDirectoryKey, .creationDateKey],
            options: .skipsSubdirectoryDescendants
        )
        
        let imageURLs = (items ?? [])
            .filter { $0.isSupportedImageURL && $0.isDirectory == false && $0.creationDate != nil }
            .sorted { $0.creationDate! > $1.creationDate! }

        return imageURLs
    }
    
    private func refreshImageList() {
        let oldURLs = imageURLs
        imageURLs = listImages()
        
        if oldURLs == imageURLs { return }
        
        let addedItems = imageURLs
            .filter { !oldURLs.contains($0) }
            .map { galleryItemForImage(at: $0) }
        
        let removedItems = oldURLs
            .filter { !imageURLs.contains($0) }
            .map { galleryItemForImage(at: $0) }

        removedItems.forEach { (item) in
            thumbnailCache.removeObject(forKey: item.URL as NSURL)
            imageSizeCache.removeObject(forKey: item.URL as NSURL)
        }
        
        let items = self.items
        delegates.forEach { (weakDelegate) in
            weakDelegate.value?.galleryManager(self, didUpdate: items, newItems: addedItems, removedItems: removedItems)
        }
    }
    
    // MARK: Items properties
    func thumbnail(for item: GalleryItem?) -> UIImage? {
        guard let item = item else { return nil }
        
        let image = thumbnailCache.object(forKey: item.thumbnailURL as NSURL) ?? UIImage(contentsOfFile: item.thumbnailURL.path)
        if image == nil {
            generateThumbAsync(for: item, fullImage: nil, tellDelegates: true)
        }
        return image
    }
    
    func dateString(for item: GalleryItem) -> String? {
        guard let date = item.URL.creationDate else { return nil }

        let formatter = DateFormatter()
        formatter.dateStyle = .long
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
    
    func imageSize(for item: GalleryItem) -> CGSize? {
        if let size = self.imageSizeCache.object(forKey: item.URL as NSURL)?.cgSizeValue {
            return size
        }
        
        guard let imageSize = UIImage.sizeOfImage(at: item.URL) else { return nil }

        imageSizeCache.setObject(NSValue(cgSize: imageSize), forKey: item.URL as NSURL)
        return imageSize
    }
    
    // MARK: Thumb
    private func generateThumbAsync(for item: GalleryItem, fullImage: UIImage?, tellDelegates: Bool) {
        guard !thumbsBeingCreated.contains(item.URL) else { return }
        thumbsBeingCreated.append(item.URL)
        
        let dequeue = { [weak self] in
            if let index = self?.thumbsBeingCreated.index(of: item.URL) {
                self?.thumbsBeingCreated.remove(at: index)
            }
        }

        thumbsQueue.addOperation {
            // this first method is a bit longer to generate images, but uses far less memory on the device
            var thumb = UIImage.thumbnailForImage(at: item.URL, maxEdgeSize: 200)
            
            // in case the first method fails we do it the old way
            if thumb == nil {
                guard let original = fullImage ?? UIImage(contentsOfFile: item.URL.path) else { return dequeue() }
                thumb = original.resizingLongestEdge(to: 200)
            }
            
            guard thumb != nil else { return dequeue() }
            
            try? thumb!
                .jpegData(compressionQuality: 0.6)?
                .write(to: item.thumbnailURL, options: .atomicWrite)
            
            self.thumbnailCache.setObject(thumb!, forKey: item.URL as NSURL)
            
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
    
    private func thumbURL(for imageURL: URL) -> URL {
        let filename = imageURL.deletingPathExtension().appendingPathExtension(kImageThumbsSuffix).lastPathComponent
        return thumbnailsFolderURL.appendingPathComponent(filename, isDirectory: false)
    }
}

private extension URL {
    var isSupportedImageURL: Bool {
        return [kImageExtensionPNG.lowercased(), kImageExtensionJPG.lowercased()].contains(pathExtension.lowercased())
    }
    
    var creationDate: Date? {
        do {
            return try resourceValues(forKeys: Set([URLResourceKey.creationDateKey])).creationDate
        }
        catch {
            print("Couldn't get creation date:", error)
            return nil
        }
    }
    
    var isDirectory: Bool? {
        do {
            return try resourceValues(forKeys: Set([URLResourceKey.isDirectoryKey])).isDirectory
        }
        catch {
            print("Couldn't get isDirectory:", error)
            return nil
        }
    }
}
