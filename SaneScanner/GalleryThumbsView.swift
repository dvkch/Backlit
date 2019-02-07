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
        view.tintColor = tintColor ?? .white
        view.backgroundColor = tintColor ?? .white
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
        collectionView.registerCell(GalleryThumbsCell.self, xib: true)
        collectionView.contentInset = .init(top: 0, left: gradientWidth, bottom: 0, right: gradientWidth)
        collectionView.scrollIndicatorInsets = collectionView.contentInset
        collectionView.setContentCompressionResistancePriority(.defaultHigh, for: .vertical)
        addSubview(collectionView)
        
        leftGradientView.layer.colors = [UIColor.white.cgColor, UIColor(white: 1, alpha: 0).cgColor]
        leftGradientView.layer.locations = [0.2, 1]
        leftGradientView.layer.startPoint = .zero
        leftGradientView.layer.endPoint = .init(x: 1, y: 0)
        addSubview(leftGradientView)
        
        rightGradientView.layer.colors = [UIColor.white.cgColor, UIColor(white: 1, alpha: 0).cgColor]
        rightGradientView.layer.locations = [0.2, 1]
        rightGradientView.layer.startPoint = .init(x: 1, y: 0)
        rightGradientView.layer.endPoint = .zero
        addSubview(rightGradientView)
        
        leftGradientView.snp.makeConstraints { (make) in
            make.top.left.bottom.equalToSuperview()
            make.width.equalTo(gradientWidth)
        }
        
        rightGradientView.snp.makeConstraints { (make) in
            make.top.right.bottom.equalToSuperview()
            make.width.equalTo(gradientWidth)
        }
        
        collectionView.snp.makeConstraints { (make) in
            make.top.equalTo(10).priority(.low)
            make.left.right.equalTo(0)
            make.centerY.equalToSuperview()
            make.height.greaterThanOrEqualTo(30)
        }
        
        GalleryManager.shared.addDelegate(self)
    }
    
    // MARK: Properties
    private let gradientWidth = CGFloat(30)
    weak var parentViewController: UIViewController?
    private let collectionViewLayout = UICollectionViewFlowLayout()
    private var collectionView: UICollectionView!
    private var galleryItems = [MHGalleryItem]()
    private let leftGradientView = SYGradientView()
    private let rightGradientView = SYGradientView()

    // MARK: Content
    override var tintColor: UIColor! {
        didSet {
            let gradientOpaqueColor = tintColor ?? .white
            
            leftGradientView.layer.colors  = [gradientOpaqueColor.cgColor, gradientOpaqueColor.withAlphaComponent(0).cgColor]
            rightGradientView.layer.colors = [gradientOpaqueColor.cgColor, gradientOpaqueColor.withAlphaComponent(0).cgColor]
        }
    }
    
    // MARK: Layout
    override func layoutSubviews() {
        super.layoutSubviews()
        centerContent()
    }
    
    private func centerContent() {
        let visibleIndexPaths = collectionView.indexPathsForVisibleItems.sorted()
        
        var leftIndexPath = visibleIndexPaths.first
        if leftIndexPath == nil && !galleryItems.isEmpty {
            leftIndexPath = IndexPath(item: 0, section: 0)
        }
        
        collectionViewLayout.minimumInteritemSpacing = 10
        
        if let leftIndexPath = leftIndexPath {
            collectionView.scrollToItem(at: leftIndexPath, at: .left, animated: false)
        }
        
        // makes sure contentSize is correct
        collectionView.layoutIfNeeded()
        
        var availableWidth = collectionView.bounds.width - collectionView.contentSize.width
        availableWidth = max(0, availableWidth - 2 * gradientWidth)
        
        // center the content
        let horizontalInset = gradientWidth + availableWidth / 2
        collectionView.contentInset = .init(top: 0, left: horizontalInset, bottom: 0, right: horizontalInset)
    }
}

extension GalleryThumbsView: GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [MHGalleryItem], newItems: [MHGalleryItem], removedItems: [MHGalleryItem]) {
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
        
        let cell = collectionView.dequeueCell(GalleryThumbsCell.self, for: indexPath)
        
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
        
        return cell
    }
}

extension GalleryThumbsView: UICollectionViewDelegateFlowLayout {
    func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> CGSize {
        let imageSize = GalleryManager.shared.imageSize(for: galleryItems[indexPath.item]) ?? CGSize(width: 100, height: 100)
        
        let bounds = collectionView.bounds.inset(by: collectionView.contentInset).inset(by: self.collectionViewLayout.sectionInset)
        
        return CGSize(width: imageSize.width * bounds.height / imageSize.height, height: bounds.height)
    }
}
