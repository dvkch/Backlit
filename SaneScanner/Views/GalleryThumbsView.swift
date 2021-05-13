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
    static func showInToolbar(of controller: UIViewController, tintColor: UIColor?) -> GalleryThumbsView {
        let initialRect = controller.navigationController?.toolbar.bounds ?? .zero
        
        let view = GalleryThumbsView(frame: initialRect)
        view.parentViewController = controller
        view.backgroundColor = tintColor ?? .cellBackground
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
        
        collectionView = UICollectionView(frame: .zero, collectionViewLayout: collectionViewLayout)
        collectionView.clipsToBounds = false
        collectionView.backgroundColor = .clear
        collectionView.dataSource = self
        collectionView.delegate = self
        if #available(iOS 11.0, *) {
            collectionView.dragDelegate = self
        }
        collectionView.registerCell(GalleryThumbnailCell.self, xib: false)
        collectionView.setContentCompressionResistancePriority(.defaultHigh, for: .vertical)
        addSubview(collectionView)
        
        collectionViewLayout.scrollDirection = scrollDirection
        
        leftGradientView.gradientLayer.locations = [0.2, 1]
        addSubview(leftGradientView)
        
        rightGradientView.gradientLayer.locations = [0.2, 1]
        addSubview(rightGradientView)
        
        updateGradientColors()
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
    private let collectionViewLayout = UICollectionViewFlowLayout()
    private var collectionView: UICollectionView!
    private var galleryItems = [GalleryItem]()
    private let leftGradientView = SYGradientView()
    private let rightGradientView = SYGradientView()
    
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

    // MARK: Content
    override var backgroundColor: UIColor? {
        didSet {
            updateGradientColors()
        }
    }
    
    override func didMoveToWindow() {
        super.didMoveToWindow()
        // need this callback too, because if we start on an iPhone layout (hidden DevicePreviewVC => window = nil => traitCollection = nil), then move
        // to large layout the gradients will be messed up
        updateGradientColors()
    }
    
    private func updateGradientColors() {
        var gradientOpaqueColor = backgroundColor ?? .white
        if #available(iOS 13.0, *) {
            gradientOpaqueColor = gradientOpaqueColor.resolvedColor(with: traitCollection)
        }

        leftGradientView.gradientLayer.colors  = [gradientOpaqueColor.cgColor, gradientOpaqueColor.withAlphaComponent(0).cgColor]
        rightGradientView.gradientLayer.colors = [gradientOpaqueColor.cgColor, gradientOpaqueColor.withAlphaComponent(0).cgColor]
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
    
    private let gradientSize = CGFloat(30)
    
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
        centerContent()
    }
    
    private func centerContent() {
        guard scrollDirection == .horizontal else { return }

        let visibleIndexPaths = collectionView.indexPathsForVisibleItems.sorted()
        
        var leftIndexPath = visibleIndexPaths.first
        if leftIndexPath == nil && !galleryItems.isEmpty {
            leftIndexPath = IndexPath(item: 0, section: 0)
        }
        
        collectionViewLayout.minimumInteritemSpacing = 10
        
        if let leftIndexPath = leftIndexPath {
            collectionView.scrollToItem(at: leftIndexPath, at: scrollDirection == .horizontal ? .left : .top, animated: false)
        }
        
        // makes sure contentSize is correct
        collectionView.layoutIfNeeded()
        
        var availableWidth = collectionView.bounds.width - collectionView.contentSize.width
        availableWidth = max(0, availableWidth - 2 * gradientSize)
        
        // center the content
        let horizontalInset = gradientSize + availableWidth / 2
        collectionView.contentInset = .init(top: 0, left: horizontalInset, bottom: 0, right: horizontalInset)
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

        leftGradientView.snp.makeConstraints { (make) in
            if scrollDirection == .horizontal {
                make.top.left.bottom.equalToSuperview()
                make.width.equalTo(gradientSize)
            } else {
                make.top.left.right.equalToSuperview()
                make.height.equalTo(gradientSize)
            }
        }
        
        rightGradientView.snp.makeConstraints { (make) in
            if scrollDirection == .horizontal {
                make.top.right.bottom.equalToSuperview()
                make.width.equalTo(gradientSize)
            } else {
                make.left.right.bottom.equalToSuperview()
                make.height.equalTo(gradientSize)
            }
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
        
        if scrollDirection == .horizontal {
            leftGradientView.gradientLayer.startPoint = .zero
            leftGradientView.gradientLayer.endPoint = .init(x: 1, y: 0)
            rightGradientView.gradientLayer.startPoint = .init(x: 1, y: 0)
            rightGradientView.gradientLayer.endPoint = .zero
        } else {
            leftGradientView.gradientLayer.startPoint = .zero
            leftGradientView.gradientLayer.endPoint = .init(x: 0, y: 1)
            rightGradientView.gradientLayer.startPoint = .init(x: 0, y: 1)
            rightGradientView.gradientLayer.endPoint = .zero
        }
        
        collectionViewLayout.invalidateLayout()
        super.updateConstraints()
    }
    
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        if #available(iOS 12.0, *), traitCollection.userInterfaceStyle != previousTraitCollection?.userInterfaceStyle {
            updateGradientColors()
        }
    }
}

extension GalleryThumbsView: GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        UIView.animate(withDuration: 0.3) {
            self.galleryItems = items
            self.collectionView?.reloadData()
            self.setNeedsLayout()
        }
    }
}

extension GalleryThumbsView: UICollectionViewDataSource {
    func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
        return galleryItems.count
    }
    
    func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        let spinnerColor = tintColor != .white ? UIColor.white : .gray
        
        let cell = collectionView.dequeueCell(GalleryThumbnailCell.self, for: indexPath)
        
        cell.update(item: galleryItems[indexPath.item], mode: .toolbar, spinnerColor: spinnerColor)
        
        // Interactive dismissal: was planning on implementing it but iOS 13 and its new modal presentation mode made it unecessary. This code sample
        // dates from when I used MHVideoPhotoGallery, it allowed the interactive dismissal to use the thumbnail corresponding to the currently displayed
        // image in the gallery as a destination rect. Sure, that would be nice to have, but who has the time?
        /*
        cell.update(
            items: galleryItems,
            index: indexPath.item,
            parentController: parentViewController?.navigationController,
            spinnerColor: spinnerColor)
        { [weak self] (index) -> UIImageView? in
            guard let self = self, index < self.galleryItems.count else { return nil }
            
            let dismissIndexPath = IndexPath(item: index, section: 0)
            self.collectionView.scrollToItem(at: dismissIndexPath, at: [.centeredVertically, .centeredHorizontally], animated: false)
            
            // needed to be sure the cell is loaded
            self.collectionView.layoutIfNeeded()
        
            let dismissCell = collectionView.cellForItem(at: dismissIndexPath) as? GalleryThumbsCell
            return dismissCell?.imageView
        }
        */
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
        let configuration = UIContextMenuConfiguration(identifier: nil, previewProvider: nil) { _ in
            let open = UIAction(title: "ACTION OPEN".localized, image: UIImage(systemName: "folder")) { _ in
                self.openGallery(at: indexPath.item)
            }
            let share = UIAction(title: "ACTION SHARE".localized, image: UIImage(systemName: "square.and.arrow.up")) { _ in
                guard let parentViewController = self.parentViewController else { return }
                UIActivityViewController.showForURLs([self.galleryItems[indexPath.item].url], in: parentViewController, sender: collectionView.cellForItem(at: indexPath), completion: nil)
            }
            return UIMenu(title: "", children: [open, share])
        }
        return configuration
    }
    #endif
}

@available(iOS 11.0, *)
extension GalleryThumbsView: UICollectionViewDragDelegate {
    func collectionView(_ collectionView: UICollectionView, itemsForBeginning session: UIDragSession, at indexPath: IndexPath) -> [UIDragItem] {
        let item = galleryItems[indexPath.item]
        return [UIDragItem(itemProvider: NSItemProvider(object: item))]
    }
}
