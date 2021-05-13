//
//  GalleryItem.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

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
