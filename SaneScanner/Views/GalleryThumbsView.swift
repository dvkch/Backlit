//
//  GalleryThumbsView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SnapKit
import SYKit

class GalleryThumbsView: UIView {

    // MARK: Init
    static func showInToolbar(of controller: UIViewController) -> GalleryThumbsView {
        let initialRect = controller.navigationController?.toolbar.bounds ?? .zero
        
        let view = GalleryThumbsView(frame: initialRect)
        view.parentViewController = controller
        view.backgroundColor = .clear
        view.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        controller.toolbarItems = [UIBarButtonItem(customView: view)]
        
        return view
    }
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    private func setup() {
        clipsToBounds = true
        
        collectionViewLayout.scrollDirection = .horizontal
        
        collectionView.clipsToBounds = false
        collectionView.backgroundColor = .clear
        collectionView.dataSource = self
        collectionView.delegate = self
        collectionView.dragDelegate = self
        collectionView.registerCell(GalleryThumbnailCell.self, xib: false)
        collectionView.setContentCompressionResistancePriority(.defaultHigh, for: .vertical)
        addSubview(collectionView)
        
        collectionViewLayout.scrollDirection = scrollDirection
        
        gradientMask.colors = [UIColor.black.withAlphaComponent(0), .black, .black, .black.withAlphaComponent(0)].map(\.cgColor)
        layer.mask = gradientMask
        
        setNeedsUpdateConstraints()
        
        GalleryManager.shared.addDelegate(self)
    }
    
    // MARK: Properties
    weak var parentViewController: UIViewController?
    var scrollDirection: UICollectionView.ScrollDirection = .horizontal {
        didSet {
            collectionViewLayout.scrollDirection = scrollDirection
            setNeedsUpdateConstraints()
        }
    }
    private var collectionViewLayout: UICollectionViewFlowLayout { collectionView.collectionViewLayout as! UICollectionViewFlowLayout }
    private let collectionView = UICollectionView(frame: .zero, collectionViewLayout: UICollectionViewFlowLayout())
    private var galleryItems = [GalleryItem]()
    private let gradientMask = CAGradientLayer()
    
    // MARK: Actions
    private func openGallery(at index: Int) {
        #if targetEnvironment(macCatalyst)
        // crashes in debug in Catalyst, but all good in release
        UIApplication.shared.open(galleryItems[index].url, options: [:], completionHandler: nil)
        #else
        let nc = GalleryNC(openedAt: index)
        parentViewController?.present(nc, animated: true, completion: nil)
        #endif
    }
    
    // MARK: Insertion animation
    private var insertedGalleryItem: GalleryItem?
    private func doInsertionAnimation(newItems: [GalleryItem], removedItems: [GalleryItem], allItems: [GalleryItem]) -> Bool {
        // make sure there is only one item added, that it's the most recent item, and that it's less than 5s old
        guard removedItems.isEmpty, newItems.count == 1, let newItem = newItems.first, newItem == allItems[0] else { return false }
        guard fabs(newItem.creationDate.timeIntervalSinceNow) < 5 else { return false }
        
        // ignore the animation if we are not fully visible
        guard parentViewController?.presentedViewController == nil else { return false }
        
        // access the source view
        guard let window = window as? ContextWindow, let sourceView = window.context?.currentPreviewView else { return false }

        // prepare the animation
        insertedGalleryItem = newItem
        defer { insertedGalleryItem = nil }

        // insert the corresponding new cell
        collectionView.performBatchUpdates({
            collectionView.insertItems(at: [IndexPath(item: 0, section: 0)])
        }, completion: { _ in
            self.insertedGalleryItem = nil
            self.collectionView.visibleCells
                .compactMap { $0 as? GalleryThumbnailCell }
                .filter { $0.item == newItem }
                .forEach { $0.isHidden = false }
        })
        
        // access the final rect of the added cell
        guard let cellRect = collectionView.layoutAttributesForItem(at: IndexPath(item: 0, section: 0))?.frame else { return false }
        
        // perform the animation
        let imageView = UIImageView(image: UIImage(contentsOfFile: newItem.thumbnailUrl.path))
        imageView.backgroundColor = .white
        imageView.isUserInteractionEnabled = false
        imageView.frame = window.convert(sourceView.cropAreaInViewBounds, from: sourceView)
        window.addSubview(imageView)

        UIView.animate(withDuration: 0.3, animations: {
            self.collectionView.contentOffset.x = -self.collectionView.contentInset.left
            imageView.frame = window.convert(cellRect, from: self.collectionView)
        }, completion: { _ in
            imageView.removeFromSuperview()
            self.insertedGalleryItem = nil
            self.collectionView.reloadData()
        })
        
        return true
    }

    // MARK: Layout
    var preferredSize: CGSize = .zero {
        didSet {
            invalidateIntrinsicContentSize()
        }
    }
    
    override var intrinsicContentSize: CGSize {
        return preferredSize
    }
    
    #if targetEnvironment(macCatalyst)
    private let gradientSize = CGFloat(30)
    #else
    private let gradientSize = CGFloat(15)
    #endif
    
    private var prevSize = CGSize.zero
    override func layoutSubviews() {
        if prevSize != bounds.size {
            prevSize = bounds.size
            // needs to be called *BEFORE* super.layoutSubviews or the layout won't refresh properly
            collectionViewLayout.invalidateLayout()
            collectionView.setNeedsLayout()
        }
        super.layoutSubviews()
        
        // needs to be called *AFTER* super.layoutSubviews or the layout won't be properly refreshed and it will fail
        updateGradient()
    }
    
    private func updateGradient() {
        gradientMask.frame = layer.bounds

        let size = scrollDirection == .horizontal ? bounds.width : bounds.height
        let sizePercent: Double = min(0.5, max(0, Double(gradientSize / size)))
        gradientMask.locations = [0, sizePercent, 1 - sizePercent, 1].map { NSNumber(value: $0) }

        if scrollDirection == .horizontal {
            gradientMask.startPoint = .zero
            gradientMask.endPoint = .init(x: 1, y: 0)
        } else {
            gradientMask.startPoint = .zero
            gradientMask.endPoint = .init(x: 0, y: 1)
            gradientMask.frame.origin.x = 1
        }
    }
    
    override func updateConstraints() {
        collectionView.alwaysBounceHorizontal = scrollDirection == .horizontal
        collectionView.alwaysBounceVertical = scrollDirection == .vertical

        if scrollDirection == .horizontal {
            collectionView.contentInset = .init(top: 0, left: gradientSize, bottom: 0, right: gradientSize)
            collectionView.scrollIndicatorInsets = .init(top: 0, left: gradientSize, bottom: 0, right: gradientSize)
        } else {
            collectionView.contentInset = .init(top: gradientSize, left: 20, bottom: gradientSize, right: 20)
            collectionView.scrollIndicatorInsets = .init(top: gradientSize, left: 0, bottom: gradientSize, right: 0)
        }

        collectionView.snp.makeConstraints { (make) in
            if scrollDirection == .horizontal {
                make.top.equalTo(10).priority(.low)
                make.left.right.equalTo(0)
                make.centerY.equalToSuperview()
                make.height.greaterThanOrEqualTo(30)
            } else {
                make.top.left.right.bottom.equalTo(0)
                make.centerX.equalToSuperview()
            }
        }
        
        collectionViewLayout.invalidateLayout()
        super.updateConstraints()
    }
}

extension GalleryThumbsView: GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        self.galleryItems = items

        let ranAnimation = doInsertionAnimation(newItems: newItems, removedItems: removedItems, allItems: items)
        if !ranAnimation {
            UIView.animate(withDuration: 0.3) {
                self.collectionView.reloadData()
                self.setNeedsLayout()
            }
        }
    }
}

extension GalleryThumbsView: UICollectionViewDataSource {
    func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
        return galleryItems.count
    }
    
    func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueCell(GalleryThumbnailCell.self, for: indexPath)
        cell.update(item: galleryItems[indexPath.item], mode: .toolbar, displayedOverTint: self.tintColor == .tint)
        cell.isHidden = (cell.item == insertedGalleryItem)
        return cell
    }
}

extension GalleryThumbsView: UICollectionViewDelegateFlowLayout {
    func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> CGSize {
        let bounds = collectionView.bounds.inset(by: collectionView.contentInset)
        let imageSize = GalleryManager.shared.imageSize(for: galleryItems[indexPath.item]) ?? CGSize(width: 100, height: 100)

        if scrollDirection == .horizontal {
            return CGSize(width: imageSize.width * bounds.height / imageSize.height, height: bounds.height)
        } else {
            return CGSize(width: bounds.width, height: imageSize.height * bounds.width / imageSize.width)
        }
    }
    
    func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
        collectionView.deselectItem(at: indexPath, animated: false)
        openGallery(at: indexPath.item)
    }
    
    #if !targetEnvironment(macCatalyst)
    @available(iOS 13.0, *)
    func collectionView(_ collectionView: UICollectionView, contextMenuConfigurationForItemAt indexPath: IndexPath, point: CGPoint) -> UIContextMenuConfiguration? {
        let item = self.galleryItems[indexPath.item]
        let configuration = UIContextMenuConfiguration(
            identifier: nil,
            previewProvider: {
                return GalleryImagePreviewVC(item: item)
            },
            actionProvider: { _ in
                let open = UIAction(title: "ACTION OPEN".localized, image: UIImage(systemName: "folder")) { _ in
                    self.openGallery(at: indexPath.item)
                }
                let share = UIAction(title: "ACTION SHARE".localized, image: UIImage(systemName: "square.and.arrow.up")) { _ in
                    guard let parentViewController = self.parentViewController else { return }
                    UIActivityViewController.showForURLs([item.url], in: parentViewController, sender: collectionView.cellForItem(at: indexPath), completion: nil)
                }
                return UIMenu(title: item.suggestedDescription ?? "", children: [open, share])
            }
        )
        configuration.indexPath = indexPath
        return configuration
    }

    @available(iOS 13.0, *)
    func collectionView(_ collectionView: UICollectionView, willPerformPreviewActionForMenuWith configuration: UIContextMenuConfiguration, animator: UIContextMenuInteractionCommitAnimating) {
        guard let indexPath = configuration.indexPath else { return }
        animator.addCompletion {
            self.openGallery(at: indexPath.item)
        }
    }
    #endif
}

extension GalleryThumbsView: UICollectionViewDragDelegate {
    func collectionView(_ collectionView: UICollectionView, itemsForBeginning session: UIDragSession, at indexPath: IndexPath) -> [UIDragItem] {
        let item = galleryItems[indexPath.item]
        return [UIDragItem(itemProvider: NSItemProvider(object: item))]
    }
}
