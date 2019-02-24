//
//  GalleryGridVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class GalleryGridVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "GALLERY OVERVIEW TITLE".localized
        
        if #available(iOS 10.0, *) {
            emptyStateLabel.adjustsFontForContentSizeCategory = true
        }
        
        collectionViewLayout.maxSize = 320/3
        collectionViewLayout.margin = 2

        collectionView.collectionViewLayout = collectionViewLayout
        collectionView.registerCell(MediaPreviewCollectionViewCell.self, xib: false)
        
        #if DEBUG
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Add test images", style: .plain, target: self, action: #selector(self.addTestImagesButtonTap))
        #endif
        
        GalleryManager.shared.addDelegate(self)
    }
    
    // MARK: Properties
    private var items = [GalleryItem]() {
        didSet {
            updateEmptyState()
            collectionView.reloadData()
        }
    }
    
    // MARK: Views
    @IBOutlet private var emptyStateView: UIView!
    @IBOutlet private var emptyStateLabel: UILabel!
    private let collectionViewLayout = GridLayout()
    @IBOutlet private var collectionView: UICollectionView!
    
    // MARK: Actions
    
    #if DEBUG
    @objc private func addTestImagesButtonTap() {
        GalleryManager.shared.createRandomTestImages(count: 20)
    }
    #endif
    
    // MARK: Content
    private func updateEmptyState() {
        let text = NSMutableAttributedString()
        text.sy_appendString("GALLERY EMPTY TITLE".localized, font: .systemFont(ofSize: 17), color: .darkGray)
        text.sy_appendString("\n\n", font: .systemFont(ofSize: 15), color: .gray)
        text.sy_appendString("GALLERY EMPTY SUBTITLE".localized, font: .systemFont(ofSize: 15), color: .gray)
        emptyStateLabel.attributedText = text
        
        emptyStateView.isHidden = !items.isEmpty
        collectionView.isHidden = items.isEmpty
    }
}

extension GalleryGridVC : UICollectionViewDataSource {
    func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
        return items.count
    }
    
    func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueCell(MediaPreviewCollectionViewCell.self, for: indexPath)
        cell.item = items[indexPath.row]
        return cell
    }
}

extension GalleryGridVC : UICollectionViewDelegate {
    func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
        collectionView.deselectItem(at: indexPath, animated: true)
        
        let imagesVC = GalleryImagesVC()
        imagesVC.initialIndex = indexPath.row
        navigationController?.pushViewController(imagesVC, animated: true)
    }
}

extension GalleryGridVC : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        self.items = items
    }
}
