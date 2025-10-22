//
//  GalleryItem.swift
//  Backlit
//
//  Created by syan on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import MobileCoreServices
import SYPictureMetadata
import UniformTypeIdentifiers

class GalleryItem: NSObject {
    
    // MARK: Properties
    let url: URL
    let thumbnailUrl: URL
    let creationDate: Date
    
    // MARK: Init
    init(url: URL, thumbnailUrl: URL) {
        self.url = url
        self.thumbnailUrl = thumbnailUrl
        self.creationDate = url.creationDate ?? Date(timeIntervalSince1970: 0)
        super.init()
    }
    
    // MARK: Equatable, Hashable
    override func isEqual(_ object: Any?) -> Bool {
        (object as? GalleryItem)?.url == url
    }
    
    override var hash: Int {
        url.hashValue
    }
}

// MARK: Item properties
extension GalleryItem {
    var lowResURL: URL? {
        #if !targetEnvironment(macCatalyst)
        GalleryImageView.lowResURLIfNeeded(forImageAt: url)
        #else
        nil
        #endif
    }
    
    static var deviceInfoCache: [URL: String?] = [:]
    var deviceInfoString: String? {
        // this is a wee bit slow, and is computed everytime we display a GalleryThumbnailCell to generate the
        // accessibility label, so let's keep a small cache of them
        if let cached = type(of: self).deviceInfoCache[url] {
            return cached
        }
        guard let metadata = try? SYMetadata(fileURL: url).metadataTIFF else {
            return nil
        }
        let deviceInfo = [metadata.make, metadata.model].removingNils().joined(separator: " ")
        type(of: self).deviceInfoCache[url] = deviceInfo
        return deviceInfo
    }
}

// MARK: File provider extension
extension GalleryItem : NSItemProviderWriting {
    static var writableTypeIdentifiersForItemProvider: [String] {
        return [UTType.image, .fileURL, .url].map(\.identifier)
    }
    
    var writableTypeIdentifiersForItemProvider: [String] {
        if let detected = try? url.resourceValues(forKeys: Set([.typeIdentifierKey])).typeIdentifier {
            return [detected] + type(of: self).writableTypeIdentifiersForItemProvider
        }

        switch url.pathExtension.lowercased() {
        case "jpeg", "jpg":
            return [UTType.jpeg.identifier] + type(of: self).writableTypeIdentifiersForItemProvider

        case "png":
            return [UTType.png.identifier] + type(of: self).writableTypeIdentifiersForItemProvider
            
        case "heic":
            return [UTType.heic.identifier] + type(of: self).writableTypeIdentifiersForItemProvider
            
        default:
            return type(of: self).writableTypeIdentifiersForItemProvider
        }
    }
    
    func loadData(withTypeIdentifier typeIdentifier: String, forItemProviderCompletionHandler completionHandler: @escaping (Data?, Error?) -> Void) -> Progress? {
        do {
            switch typeIdentifier {
            case UTType.jpeg.identifier, UTType.png.identifier, UTType.heic.identifier, UTType.image.identifier:
                let data = try Data(contentsOf: url, options: .mappedIfSafe)
                completionHandler(data, nil)
                
            default:
                completionHandler(url.dataRepresentation, nil)
            }
        }
        catch {
            completionHandler(nil, error)
        }
        
        return nil
    }
}
