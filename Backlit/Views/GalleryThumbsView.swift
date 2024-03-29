//
//  GalleryThumbsView.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SnapKit
import SYKit
import DiffableDataSources

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
        
        collectionViewLayout.bounciness = 2
        collectionViewLayout.scrollDirection = scrollDirection
        collectionViewLayout.minimumInteritemSpacing = 10

        collectionView.clipsToBounds = false
        collectionView.backgroundColor = .clear
        collectionView.dataSource = galleryDataSource
        collectionView.delegate = self
        collectionView.dragDelegate = self
        collectionView.registerCell(GalleryThumbnailCell.self, xib: false)
        collectionView.setContentCompressionResistancePriority(.defaultHigh, for: .vertical)
        addSubview(collectionView)
        
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
    private var collectionViewLayout: BouncyLayout { collectionView.collectionViewLayout as! BouncyLayout }
    private let collectionView = UICollectionView(frame: .zero, collectionViewLayout: BouncyLayout())
    private lazy var galleryDataSource = CollectionViewDiffableDataSource<GalleryGroup, GalleryItem>(collectionView: collectionView, viewsProvider: self)
    private let gradientMask = CAGradientLayer()
    
    // MARK: Actions
    private func openGallery(for item: GalleryItem) {
        #if targetEnvironment(macCatalyst)
        // crashes in debug in Catalyst, but all good in release
        UIApplication.shared.open(item.url, options: [:], completionHandler: nil)
        #else
        let nc = GalleryNC(openedOn: item)
        parentViewController?.present(nc, animated: true, completion: nil)
        #endif
    }
    
    private func updateGalleryItems(using items: [GalleryItem], animated: Bool) {
        var snapshot = DiffableDataSourceSnapshot<GalleryGroup, GalleryItem>()
        snapshot.appendSections([GalleryGroup.stable()])
        snapshot.appendItems(items.reversed())
        galleryDataSource.apply(snapshot, animatingDifferences: animated)
    }
    
    // MARK: Insertion animation
    private var insertedGalleryItem: GalleryItem?
    private func doInsertionAnimation(newItems: [GalleryItem], removedItems: [GalleryItem], allItems: [GalleryItem], reloadData: @escaping () -> ()) {
        // respect the user's choice
        guard !UIAccessibility.isReduceMotionEnabled else {
            return reloadData()
        }
        
        // make sure there is only one item added, that it's the most recent item, and that it's less than 5s old
        guard removedItems.isEmpty, newItems.count == 1, let newItem = newItems.first, newItem == allItems.last else {
            return reloadData()
        }
        guard fabs(newItem.creationDate.timeIntervalSinceNow) < 5 else {
            return reloadData()
        }
        
        // ignore the animation if we are not fully visible
        guard parentViewController?.presentedViewController == nil else {
            return reloadData()
        }
        
        // access the source view
        guard let window = window as? ContextWindow, let sourceView = window.context?.currentPreviewView else {
            return reloadData()
        }

        // scroll back to the beginning
        // it is unfortunately not possible to animate the contentOffset during the animation, the cells
        // will be reused and disappear while scrolling. so we scroll first, then actually do the animation
        // cf: https://stackoverflow.com/q/49818021/1439489
        if galleryDataSource.totalCount > 0 && !collectionView.indexPathsForVisibleItems.contains(IndexPath(item: 0, section: 0)) {
            collectionView.scrollToItem(at: IndexPath(item: 0, section: 0), at: [.left, .top], animated: true)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
                self.doInsertionAnimation(
                    newItems: newItems, removedItems: removedItems, allItems: allItems, reloadData: reloadData
                )
            }
            return
        }

        // prepare the animation; this is done BEFORE the collectionView snapshot is applied, so that the cell is hidden
        // wehn it appears
        insertedGalleryItem = newItem

        // let's wait for the reload to be done, so we can get the proper cellRect
        reloadData()

        // obtain the newly inserted cell rect
        let cellRect = self.collectionView.layoutAttributesForItem(at: IndexPath(item: 0, section: 0))?.frame ?? .zero

        // perform the animation
        let imageView = UIImageView(image: UIImage(contentsOfFile: newItem.thumbnailUrl.path))
        imageView.backgroundColor = .white
        imageView.isUserInteractionEnabled = false
        imageView.frame = window.convert(sourceView.cropAreaInViewBounds, from: sourceView)
        window.addSubview(imageView)

        UIView.animate(withDuration: 0.5, animations: {
            imageView.frame = window.convert(cellRect, from: self.collectionView)
        }, completion: { _ in
            imageView.removeFromSuperview()
            self.insertedGalleryItem = nil
            self.collectionView.performBatchUpdates {
                self.collectionView.reloadItems(at: [IndexPath(item: 0, section: 0)])
            }
        })
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
            let scrollIndicatorHeight: CGFloat = traitCollection.verticalSizeClass == .compact ? 0 : 12
            collectionView.contentInset = .init(top: 0, left: gradientSize, bottom: scrollIndicatorHeight, right: gradientSize)
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
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        doInsertionAnimation(newItems: newItems, removedItems: removedItems, allItems: items) {
            self.updateGalleryItems(using: items, animated: true)
        }
    }
}

extension GalleryThumbsView: CollectionViewDiffableDataSourceViewsProvider {
    typealias SectionIdentifier = GalleryGroup
    typealias ItemIdentifier = GalleryItem
    
    func collectionView(_ collectionView: UICollectionView, cellForItem item: GalleryItem, at indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueCell(GalleryThumbnailCell.self, for: indexPath)
        cell.update(item: item, displayedOverTint: self.tintColor == .tint)
        cell.isHidden = (item == insertedGalleryItem)
        cell.accessibilityIdentifier = "gallery-item-\(indexPath.item)"
        return cell
    }
    
    func collectionView(_ collectionView: UICollectionView, viewForSupplementaryElementOfKind kind: String, at indexPath: IndexPath) -> UICollectionReusableView {
        return UICollectionReusableView()
    }
}

extension GalleryThumbsView: UICollectionViewDelegateFlowLayout {
    func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> CGSize {
        guard let item = galleryDataSource.itemIdentifier(for: indexPath) else { return .zero }
        let bounds = collectionView.bounds.inset(by: collectionView.contentInset)
        let imageSize = GalleryManager.shared.imageSize(for: item) ?? CGSize(width: 100, height: 100)

        if scrollDirection == .horizontal {
            return CGSize(width: imageSize.width * bounds.height / imageSize.height, height: bounds.height)
        } else {
            return CGSize(width: bounds.width, height: imageSize.height * bounds.width / imageSize.width)
        }
    }
    
    func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
        collectionView.deselectItem(at: indexPath, animated: false)
        if let item = galleryDataSource.itemIdentifier(for: indexPath) {
            openGallery(for: item)
        }
    }
    
    #if !targetEnvironment(macCatalyst)
    @available(iOS 13.0, *)
    func collectionView(_ collectionView: UICollectionView, contextMenuConfigurationForItemAt indexPath: IndexPath, point: CGPoint) -> UIContextMenuConfiguration? {
        guard let item = galleryDataSource.itemIdentifier(for: indexPath) else { return nil }
        guard let cell = collectionView.cellForItem(at: indexPath), let parentViewController else { return nil }
        let configuration = item.contextMenuConfiguration(
            for: parentViewController,
            sender: cell,
            openGallery: { self.openGallery(for: item) }
        )
        configuration.indexPath = indexPath
        return configuration
    }

    @available(iOS 13.0, *)
    func collectionView(_ collectionView: UICollectionView, willPerformPreviewActionForMenuWith configuration: UIContextMenuConfiguration, animator: UIContextMenuInteractionCommitAnimating) {
        guard let indexPath = configuration.indexPath, let item = galleryDataSource.itemIdentifier(for: indexPath) else { return }
        animator.addCompletion {
            self.openGallery(for: item)
        }
    }
    #endif
}

extension GalleryThumbsView: UICollectionViewDragDelegate {
    func collectionView(_ collectionView: UICollectionView, itemsForBeginning session: UIDragSession, at indexPath: IndexPath) -> [UIDragItem] {
        guard let item = galleryDataSource.itemIdentifier(for: indexPath) else { return [] }
        return [UIDragItem(itemProvider: NSItemProvider(object: item))]
    }
}
