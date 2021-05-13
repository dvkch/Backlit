//
//  GalleryItem.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import MobileCoreServices

class GalleryItem: NSObject {
    let url: URL
    let thumbnailUrl: URL
    
    init(url: URL, thumbnailUrl: URL) {
        self.url = url
        self.thumbnailUrl = thumbnailUrl
        super.init()
    }
    
    override func isEqual(_ object: Any?) -> Bool {
        (object as? GalleryItem)?.url == url
    }
}

extension GalleryItem : NSItemProviderWriting {
    static var writableTypeIdentifiersForItemProvider: [String] {
        return [String(kUTTypeImage), String(kUTTypeFileURL), String(kUTTypeURL)]
    }
    
    var writableTypeIdentifiersForItemProvider: [String] {
        switch url.pathExtension.lowercased() {
        case "jpeg", "jpg":
            return [String(kUTTypeJPEG)] + type(of: self).writableTypeIdentifiersForItemProvider

        case "png":
            return [String(kUTTypePNG)] + type(of: self).writableTypeIdentifiersForItemProvider
            
        default:
            return type(of: self).writableTypeIdentifiersForItemProvider
        }
    }
    
    func loadData(withTypeIdentifier typeIdentifier: String, forItemProviderCompletionHandler completionHandler: @escaping (Data?, Error?) -> Void) -> Progress? {
        do {
            switch typeIdentifier {
            case String(kUTTypeJPEG), String(kUTTypePNG), String(kUTTypeImage):
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
