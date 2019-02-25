//
//  GalleryImageVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

protocol GalleryImageVCDelegate: NSObjectProtocol {
    func galleryImageVC(singleTapped: GalleryImageVC)
}

class GalleryImageVC: UIViewController {

    // TODO: interactive dismissal
    
    // MARK: Init
    init(item: GalleryItem, index: Int) {
        self.item = item
        self.galleryIndex = index
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: UIViewController
    override func viewDidLoad() {
        super.viewDidLoad()
        
        let singleTap = UITapGestureRecognizer(target: self, action: #selector(self.singleTapRecognized(sender:)))
        view.addGestureRecognizer(singleTap)
        
        let doubleTap = UITapGestureRecognizer(target: self, action: #selector(self.doubleTapRecognized(sender:)))
        doubleTap.numberOfTapsRequired = 2
        scrollView.addGestureRecognizer(doubleTap)
        
        singleTap.require(toFail: doubleTap)

        scrollView.delegate = self
        scrollView.showsHorizontalScrollIndicator = false
        scrollView.showsVerticalScrollIndicator = false
        view.addSubview(scrollView)
        scrollView.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        imageView.backgroundColor = .red
        imageView.contentMode = .scaleAspectFit
        scrollView.addSubview(imageView)
        
        view.setNeedsUpdateConstraints()
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        // lazy loading only when view will appear for the first time
        if imageView.image == nil {
            imageView.image = UIImage(contentsOfFile: item.URL.path)
            view.setNeedsUpdateConstraints()
            updateZoomScales(reset: true)
        }
    }
    
    // MARK: Properties
    weak var delegate: GalleryImageVCDelegate?
    let item: GalleryItem
    let galleryIndex: Int
    
    // MARK: Views
    private let scrollView = UIScrollView()
    private let imageView = UIImageView()
    
    // MARK: Actions
    @objc private func singleTapRecognized(sender: UITapGestureRecognizer) {
        delegate?.galleryImageVC(singleTapped: self)
    }
    
    @objc private func doubleTapRecognized(sender: UITapGestureRecognizer) {
        UIView.animate(withDuration: 0.3) {
            if self.scrollView.zoomScale > self.scrollView.minimumZoomScale {
                self.scrollView.zoomScale = self.scrollView.minimumZoomScale
            }
            else {
                // TODO: zoom to specific point
                self.scrollView.zoomScale = 1
            }
            self.updateScrollViewInsets()
        }
    }
    
    // MARK: Layout
    override func updateViewConstraints() {
        let size = imageView.image?.size ?? GalleryManager.shared.imageSize(for: item) ?? CGSize(width: 0, height: 0)
        
        imageView.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
            make.size.equalTo(size)
        }

        super.updateViewConstraints()
    }
    
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        updateZoomScales(reset: false)
        updateScrollViewInsets()
    }

    private func updateZoomScales(reset: Bool) {
        view.layoutIfNeeded()
        scrollView.layoutIfNeeded()
        
        if imageView.bounds.width == 0 || imageView.bounds.height == 0 {
            scrollView.minimumZoomScale = 1
            scrollView.maximumZoomScale = 1
            scrollView.zoomScale = 1
            return
        }
        
        let fitScale = min(
            scrollView.bounds.width / imageView.bounds.width,
            scrollView.bounds.height / imageView.bounds.height
        )
        
        let prevScale = scrollView.zoomScale
        scrollView.minimumZoomScale = fitScale
        scrollView.maximumZoomScale = max(1, fitScale)
        
        if reset {
            scrollView.zoomScale = fitScale
        } else {
            scrollView.zoomScale = min(scrollView.maximumZoomScale, max(scrollView.minimumZoomScale, prevScale))
        }
    }
    
    private func updateScrollViewInsets() {
        var bounds = scrollView.frame // don't use bounds, it changes with the contentInsets and will result in an endless loop crash
        bounds.origin = .zero
        // TODO: cleanup & move to dedicated class
        //if #available(iOS 11.0, *) {
        //    bounds = bounds.inset(by: scrollView.safeAreaInsets)
        //}
        
        scrollView.contentInset.left = bounds.minX + (bounds.width  - scrollView.contentSize.width)  / 2
        scrollView.contentInset.top  = bounds.minY + (bounds.height - scrollView.contentSize.height) / 2
    }
}

extension GalleryImageVC : UIScrollViewDelegate {
    func viewForZooming(in scrollView: UIScrollView) -> UIView? {
        return imageView
    }
    
    func scrollViewDidScroll(_ scrollView: UIScrollView) {
        updateScrollViewInsets()
    }
}
