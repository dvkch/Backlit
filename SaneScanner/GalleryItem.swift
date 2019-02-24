//
//  GalleryItem.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

struct GalleryItem {
    let URL: URL
    let thumbnailURL: URL
}

extension GalleryItem : Equatable {
    static func ==(lhs: GalleryItem, rhs: GalleryItem) -> Bool {
        return lhs.URL == rhs.URL
    }
}
