//
//  GalleryItem.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import MobileCoreServices
import SYPictureMetadata

class GalleryItem: NSObject {
    let url: URL
    let thumbnailUrl: URL
    let creationDate: Date

    init(url: URL, thumbnailUrl: URL) {
        self.url = url
        self.thumbnailUrl = thumbnailUrl
        self.creationDate = url.creationDate ?? Date(timeIntervalSince1970: 0)
        super.init()
    }
    
    override func isEqual(_ object: Any?) -> Bool {
        (object as? GalleryItem)?.url == url
    }
}

// MARK: Item properties
extension GalleryItem {
    private static let dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateStyle = .long
        formatter.timeStyle = .short
        formatter.locale = .autoupdatingCurrent
        formatter.timeZone = .autoupdatingCurrent
        formatter.formattingContext = .standalone
        return formatter
    }()

    private static let relativeDateFormatter: Formatter? = {
        if #available(iOS 13.0, *) {
            let formatter = RelativeDateTimeFormatter()
            formatter.unitsStyle = .full
            formatter.locale = .autoupdatingCurrent
            formatter.calendar = .autoupdatingCurrent
            formatter.dateTimeStyle = .named
            formatter.formattingContext = .standalone
            return formatter
        } else {
            return nil
        }
    }()
    
    func creationDateString(allowRelative: Bool) -> String {
        if allowRelative, Date().timeIntervalSince(creationDate) < 7 * 24 * 3600,
           #available(iOS 13.0, *), let formatter = type(of: self).relativeDateFormatter as? RelativeDateTimeFormatter
        {
            return formatter.localizedString(for: creationDate, relativeTo: Date())
        }
        else {
            return type(of: self).dateFormatter.string(from: creationDate)
        }
    }
    
    var deviceInfoString: String? {
        guard let metadata = try? SYMetadata(fileURL: url).metadataTIFF else {
            return nil
        }
        return [metadata.make, metadata.model].removingNils().joined(separator: " ")
    }
    
    // MARK: Generated labels
    func suggestedDescription(separator: String) -> String? {
        return [
            creationDateString(allowRelative: true),
            deviceInfoString
        ].removingNils().joined(separator: separator)
    }

    var suggestedAccessibilityLabel: String? {
        return [
            "GALLERY ITEM".localized,
            suggestedDescription(separator: "; ")
        ].removingNils().joined(separator: "; ")
    }
}

// MARK: File provider extension
extension GalleryItem : NSItemProviderWriting {
    static var writableTypeIdentifiersForItemProvider: [String] {
        return [String(kUTTypeImage), String(kUTTypeFileURL), String(kUTTypeURL)]
    }
    
    var writableTypeIdentifiersForItemProvider: [String] {
        if let detected = try? url.resourceValues(forKeys: Set([.typeIdentifierKey])).typeIdentifier {
            return [detected] + type(of: self).writableTypeIdentifiersForItemProvider
        }

        switch url.pathExtension.lowercased() {
        case "jpeg", "jpg":
            return [String(kUTTypeJPEG)] + type(of: self).writableTypeIdentifiersForItemProvider

        case "png":
            return [String(kUTTypePNG)] + type(of: self).writableTypeIdentifiersForItemProvider
            
        case "heic":
            return ["public.heic"] + type(of: self).writableTypeIdentifiersForItemProvider
            
        default:
            return type(of: self).writableTypeIdentifiersForItemProvider
        }
    }
    
    func loadData(withTypeIdentifier typeIdentifier: String, forItemProviderCompletionHandler completionHandler: @escaping (Data?, Error?) -> Void) -> Progress? {
        do {
            switch typeIdentifier {
            case String(kUTTypeJPEG), String(kUTTypePNG), "public.heic", String(kUTTypeImage):
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
