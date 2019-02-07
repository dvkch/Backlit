//
//  GalleryManager.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import MHVideoPhotoGallery
import SYPictureMetadata
import SYKit
import SDWebImage
import SYOperationQueue
import DirectoryWatcher

@objc protocol GalleryManagerDelegate: AnyObject {
    @objc optional func galleryManager(_ manager: GalleryManager, didUpdate items: [MHGalleryItem], newItems: [MHGalleryItem], removedItems: [MHGalleryItem])
    @objc optional func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: MHGalleryItem)
}

private let kImageExtensionPNG  = "png"
private let kImageExtensionJPG  = "jpg"
private let kImageThumbsSuffix  = "thumbs.jpg"
private let kImageThumbsFolder  = "thumbs"
private let kImagePDFFolder     = "PDF"
private let kImagePDFPrefix     = "SaneScanner_"
private let kImageExtensionPDF  = "pdf"

@objc class GalleryManager: NSObject {
    
    // MARK: Init
    @objc static let shared = GalleryManager()
    
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
        thumbsQueue.mode = SYOperationQueueModeLIFO // TODO: cleanup
        
        watcher = DirectoryWatcher.watch(URL(fileURLWithPath: SYTools.documentsPath)) { [weak self] in
            self?.refreshImageList()
        }
        
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
    private var delegates = [WeakValue<NSObject & GalleryManagerDelegate>]()

    @objc func addDelegate(_ delegate: NSObject & GalleryManagerDelegate) {
        delegates.append(WeakValue(delegate))
        delegate.galleryManager?(self, didUpdate: items, newItems: [], removedItems: [])
    }
    
    @objc func removeDelegate(_ delegate: NSObject & GalleryManagerDelegate) {
        // don't use in the delegate's -dealloc method
        delegates.removeAll { (weakDelegate: WeakValue<NSObject & GalleryManagerDelegate>) -> Bool in
            if let weakValue = weakDelegate.value {
                return weakValue == delegate
            }
            return true
        }
    }
    
    // MARK: Items management
    private var imageURLs = [URL]()
    
    private func galleryItemForImage(at url: URL) -> MHGalleryItem {
        let item = MHGalleryItem(
            url: url,
            thumbnailURL: thumbURL(for: url)
        )!
        return item
    }
    
    @objc var items: [MHGalleryItem] {
        return imageURLs.map { galleryItemForImage(at: $0) }
    }
    
    @objc func addImage(_ image: UIImage, metadata: SYMetadata?) -> MHGalleryItem? {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        var fileURL = URL(fileURLWithPath: SYTools.documentsPath).appendingPathComponent(formatter.string(from: Date()), isDirectory: false)
        
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
    
    @objc func deleteItem(_ item: MHGalleryItem) {
        try? FileManager.default.removeItem(at: item.url)
        try? FileManager.default.removeItem(at: item.thumbnailURL)
    }

    private func listImages() -> [URL] {
        let items = try? FileManager.default.contentsOfDirectory(
            at: URL(fileURLWithPath: SYTools.documentsPath),
            includingPropertiesForKeys: [.isDirectoryKey, .creationDateKey],
            options: .skipsSubdirectoryDescendants
        )
        
        let imageURLs = (items ?? [])
            .filter { $0.isSupportedImageURL && $0.isDirectory == true && $0.creationDate != nil }
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
            thumbnailCache.removeObject(forKey: item.url as NSURL)
            imageSizeCache.removeObject(forKey: item.url as NSURL)
        }
        
        let items = self.items
        delegates.forEach { (weakDelegate) in
            weakDelegate.value?.galleryManager?(self, didUpdate: items, newItems: addedItems, removedItems: removedItems)
        }
    }
    
    // MARK: Items properties
    @objc func thumbnail(for item: MHGalleryItem) -> UIImage? {
        let image = thumbnailCache.object(forKey: item.thumbnailURL as NSURL) ?? UIImage(contentsOfFile: item.thumbnailURL!.path)
        if image == nil {
            generateThumbAsync(for: item, fullImage: nil, tellDelegates: true)
        }
        return image
    }
    
    @objc func dateString(for item: MHGalleryItem) -> String? {
        // TODO: use url.creationDate?
        guard let attributes = try? FileManager.default.attributesOfItem(atPath: item.url.path) else { return nil }
        guard let date = attributes[.creationDate] as? Date else { return nil }
        
        let formatter = DateFormatter()
        formatter.dateStyle = .long
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
    
    func imageSize(for item: MHGalleryItem) -> CGSize? {
        if let size = self.imageSizeCache.object(forKey: item.url as NSURL)?.cgSizeValue {
            return size
        }
        
        guard let imageSize = UIImage.sizeOfImage(at: item.url) else { return nil }

        imageSizeCache.setObject(NSValue(cgSize: imageSize), forKey: item.url as NSURL)
        return imageSize
    }
    
    // MARK: Thumb
    private func generateThumbAsync(for item: MHGalleryItem, fullImage: UIImage?, tellDelegates: Bool) {
        guard !thumbsBeingCreated.contains(item.url) else { return }
        thumbsBeingCreated.append(item.url)
        
        let dequeue = { [weak self] in
            if let index = self?.thumbsBeingCreated.index(of: item.url) {
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
                .write(to: item.thumbnailURL, options: .atomicWrite)
            
            self.thumbnailCache.setObject(thumb!, forKey: item.url as NSURL)
            
            dequeue()
            
            guard tellDelegates else { return }
            
            DispatchQueue.main.async {
                self.delegates.forEach { (weakDelegate) in
                    weakDelegate.value?.galleryManager?(self, didCreate: thumb!, for: item)
                }
            }
        }
    }
    
    
    // MARK: PDF
    @objc func tempPDFUrl() -> URL {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd_HH-mm-ss"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        
        let filename = kImagePDFPrefix + formatter.string(from: Date())
        return pdfFolderURL.appendingPathComponent(filename, isDirectory: false).appendingPathExtension(kImageExtensionPDF)
    }
    
    @objc func deleteTempPDF() {
        // remove pdf files in cache / PDF
        let tempURLs = try? FileManager.default.contentsOfDirectory(at: pdfFolderURL, includingPropertiesForKeys: nil, options: .skipsSubdirectoryDescendants)
        
        tempURLs?.forEach { (url) in
            try? FileManager.default.removeItem(at: url)
        }
    }

    // MARK: Paths
    private var thumbnailsFolderURL: URL {
        return URL(fileURLWithPath: SYTools.cachePath, isDirectory: true).appendingPathComponent(kImageThumbsFolder, isDirectory: true)
    }
    
    private var pdfFolderURL: URL {
        return URL(fileURLWithPath: SYTools.cachePath, isDirectory: true).appendingPathComponent(kImagePDFFolder, isDirectory: true)
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
