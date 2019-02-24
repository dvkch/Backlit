//
//  MediaPreviewCollectionViewCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import MHVideoPhotoGallery

class MediaPreviewCollectionViewCell: MHMediaPreviewCollectionViewCell {

    override init(frame: CGRect) {
        super.init(frame: frame)
        GalleryManager.shared.addDelegate(self)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    var item: GalleryItem? {
        didSet {
            if let item = item {
                galleryItem = MHGalleryItem(url: item.URL, thumbnailURL: item.thumbnailURL)
            } else {
                galleryItem = nil
            }
            updateContent()
        }
    }
    
    // MARK: Content
    private func updateContent() {
        let thumb = item.map(GalleryManager.shared.thumbnail(for:))
        thumbnail.image = thumb ?? nil
        thumbnail.setNeedsLayout()
        
        if thumb != nil {
            activityIndicator.stopAnimating()
            thumbnail.backgroundColor = .white
        }
        else {
            activityIndicator.startAnimating()
            thumbnail.backgroundColor = .lightGray
        }
    }
}

extension MediaPreviewCollectionViewCell : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) { }
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) {
        guard item == self.item else { return }
        
        self.thumbnail.image = thumbnail
        self.thumbnail.setNeedsLayout()
        self.thumbnail.backgroundColor = .white
        activityIndicator.stopAnimating()
    }
}


