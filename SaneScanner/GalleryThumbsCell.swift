//
//  GalleryThumbsCell.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import MHVideoPhotoGallery
import SnapKit

class GalleryThumbsCell: UICollectionViewCell {
    
    override func awakeFromNib() {
        super.awakeFromNib()
        
        contentView.layer.shadowColor = UIColor.black.cgColor
        contentView.layer.shadowOffset = .zero
        contentView.layer.shadowRadius = 2
        contentView.layer.rasterizationScale = UIScreen.main.scale
        
        imageView.shoudlUsePanGestureReconizer = false
        
        GalleryManager.shared.addDelegate(self)
    }

    // MARK: Properties
    private var item: MHGalleryItem?
    
    // MARK: View
    @IBOutlet private(set) var imageView: MHPresenterImageView!
    @IBOutlet private var spinner: UIActivityIndicatorView!

    // MARK: Content
    func update(items: [MHGalleryItem], index: Int, parentController: UIViewController?, spinnerColor: UIColor, dismiss: ((_ index: Int) -> UIImageView?)?) {
        weak var weakParentVC = parentController
        
        spinner?.color = spinnerColor
        
        let item = items[index]
        self.item = item
        updateImage(GalleryManager.shared.thumbnail(for: item))
        imageView.uiCustomization = MHUICustomization.sy_defaultTheme
        imageView.galleryClass = GalleryViewController.self
        
        imageView.setInseractiveGalleryPresentionWithItems(items, currentImageIndex: index, currentViewController: parentController)
        { (index, image, transition, viewMode) in
            if viewMode == .overView {
                weakParentVC?.dismiss(animated: true, completion: nil)
            } else {
                weakParentVC?.presentedViewController?.dismiss(animated: true, dismiss: dismiss?(index), completion: nil)
            }
        }
    }

    private func updateImage(_ image: UIImage?) {
        imageView.image = image
        if image != nil {
            spinner?.stopAnimating()
            contentView.layer.shadowOpacity = 0.6
            contentView.layer.shouldRasterize = true
            // backgroundColor = .red
        }
        else {
            spinner?.startAnimating()
            contentView.layer.shadowOpacity = 0
            contentView.layer.shouldRasterize = false
            // backgroundColor = .green
        }
    }

}

extension GalleryThumbsCell : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: MHGalleryItem) {
        guard item == self.item else { return }
        updateImage(thumbnail)
    }
}
