//
//  GalleryThumbnailCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class GalleryThumbnailCell: UICollectionViewCell {

    override init(frame: CGRect) {
        super.init(frame: frame)
        
        imageView.contentMode = .scaleAspectFill
        imageView.clipsToBounds = true
        imageView.layer.minificationFilter = .trilinear
        contentView.addSubview(imageView)
        imageView.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        spinner.hidesWhenStopped = true
        contentView.addSubview(spinner)
        spinner.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        GalleryManager.shared.addDelegate(self)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    var item: GalleryItem? {
        didSet {
            updateContent()
        }
    }
    
    // MARK: Views
    private let imageView = UIImageView()
    private let spinner = UIActivityIndicatorView(style: .white)

    // MARK: Content
    private func updateContent() {
        let thumb = item.map(GalleryManager.shared.thumbnail(for:))
        imageView.image = thumb ?? nil
        imageView.setNeedsLayout()
        
        if thumb != nil {
            spinner.stopAnimating()
            imageView.backgroundColor = .white
        }
        else {
            spinner.startAnimating()
            imageView.backgroundColor = .lightGray
        }
    }
}

extension GalleryThumbnailCell : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) { }
    
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) {
        guard item == self.item else { return }
        
        imageView.image = thumbnail
        imageView.setNeedsLayout()
        imageView.backgroundColor = .white
        spinner.stopAnimating()
    }
}


