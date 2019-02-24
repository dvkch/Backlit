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
    private var item: GalleryItem?
    private var mode: Mode = .gallery
    
    enum Mode {
        case gallery, toolbar
    }
    
    // MARK: Views
    private let imageView = UIImageView()
    private let spinner = UIActivityIndicatorView(style: .white)

    // MARK: Content
    func update(item: GalleryItem, mode: Mode, spinnerColor: UIColor) {
        spinner.color = spinnerColor
        self.item = item
        self.mode = mode
        
        updateThumbnail(nil)
        
        /*
        weak var weakParentVC = parentController
        
        // TODO: cleanup
        // imageView.uiCustomization = MHUICustomization.sy_defaultTheme
        // imageView.galleryClass = GalleryViewController.self
        imageView.setInseractiveGalleryPresentionWithItems(items, currentImageIndex: index, currentViewController: parentController)
            { (index, image, transition, viewMode) in
            if viewMode == .overView {
            weakParentVC?.dismiss(animated: true, completion: nil)
            } else {
            weakParentVC?.presentedViewController?.dismiss(animated: true, dismiss: dismiss?(index), completion: nil)
            }
        }
        */
    }
    
    private func updateThumbnail(_ thumbnail: UIImage?) {
        imageView.image = thumbnail ?? GalleryManager.shared.thumbnail(for: item)
        updateStyle()
    }
    
    private func updateStyle() {
        if imageView.image != nil {
            spinner.stopAnimating()
            imageView.backgroundColor = .white
        }
        else {
            spinner.startAnimating()
            imageView.backgroundColor = .lightGray
        }

        switch mode {
        case .gallery:
            contentView.layer.shadowColor = nil
            contentView.layer.shouldRasterize = false
            if imageView.image != nil {
                imageView.backgroundColor = .white
            }
            else {
                imageView.backgroundColor = .lightGray
            }

        case .toolbar:
            contentView.layer.shadowColor = UIColor.black.cgColor
            contentView.layer.shadowOffset = .zero
            contentView.layer.shadowRadius = 2
            contentView.layer.rasterizationScale = UIScreen.main.scale
            
            if imageView.image != nil {
                contentView.layer.shadowOpacity = 0.6
                contentView.layer.shouldRasterize = true
            } else {
                contentView.layer.shadowOpacity = 0
                contentView.layer.shouldRasterize = false
            }
        }
    }
}

extension GalleryThumbnailCell : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) { }
    
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) {
        guard item == self.item else { return }
        updateThumbnail(thumbnail)
    }
}


