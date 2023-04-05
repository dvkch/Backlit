//
//  GalleryImageVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import VisionKit

#if !targetEnvironment(macCatalyst)
protocol GalleryImageVCDelegate: NSObjectProtocol {
    func galleryImageVC(singleTapped: GalleryImageVC)
}

class GalleryImageVC: UIViewController {

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

        scrollView.contentInsetAdjustmentBehavior = .never
        scrollView.delegate = self
        scrollView.showsHorizontalScrollIndicator = false
        scrollView.showsVerticalScrollIndicator = false
        view.addSubview(scrollView)
        scrollView.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        imageView.contentMode = .scaleAspectFit
        scrollView.addSubview(imageView)
        
        updateZoomScales()
        view.setNeedsUpdateConstraints()
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        // lazy loading instead of doing it in viewDidLoad
        if imageView.imageURL == nil {
            imageView.imageURL = item.url
            if #available(iOS 16.0, *) {
                startImageAnalysis()
            }
        }
        view.setNeedsUpdateConstraints()
    }
    
    deinit {
        if #available(iOS 16.0, *) {
            // weirdly so there is a crash when mixing CATiledLayer and ImageAnalysisInteraction so when
            // the controller is closing for good we remove it to prevent the crash (tested on iOS 16.2)
            // the crash happens in objc_msgSend for -[UITextSelectionView removeFromSuperview].
            // surprisingly, doing this in GalleryTiledImageView.deinit doesn't solve the crash
            removeImageInteraction()
        }
    }
    
    // MARK: Properties
    weak var delegate: GalleryImageVCDelegate?
    let item: GalleryItem
    let galleryIndex: Int
    private var hasAlreadyComputedScaleOnce = false
    private var untypedImageInteraction: AnyObject?
    
    // MARK: Views
    private let scrollView = UIScrollView()
    private let imageView = GalleryImageView()
    
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
                let size = CGSize(width: 1, height: 1)
                
                var point = sender.location(in: self.imageView)
                point.x = min(max(0, point.x), self.imageView.bounds.width - size.width) * self.scrollView.maximumZoomScale
                point.y = min(max(0, point.y), self.imageView.bounds.height - size.height) * self.scrollView.maximumZoomScale

                self.scrollView.zoomScale = self.scrollView.maximumZoomScale
                self.scrollView.scrollRectToVisible(CGRect(origin: point, size: size), animated: false)
            }
            self.updateScrollViewInsets()
        }
    }
    
    // MARK: Layout
    override func updateViewConstraints() {
        imageView.snp.remakeConstraints { (make) in
            make.edges.equalToSuperview()
            make.size.equalTo(imageView.displayedImageSize)
        }

        super.updateViewConstraints()
    }
    
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        updateZoomScales()
    }
    
    var zoomingBounds: CGRect {
        // use frame instead of bounds, the latter changes with the contentInsets and will result in an endless loop
        var bounds = scrollView.frame
        bounds.origin = .zero
        
        if !sy_isModal {
            // when we're not modal we can't dismiss the nav & tool bars, so we take them into account
            bounds = bounds.inset(by: scrollView.safeAreaInsets)
        }
        
        return bounds
    }

    private func updateZoomScales() {
        view.layoutIfNeeded()
        scrollView.layoutIfNeeded()
        
        if imageView.imageURL == nil {
            scrollView.minimumZoomScale = 1
            scrollView.maximumZoomScale = 1
            scrollView.zoomScale = 1
            return
        }
        
        let zoomBounds = zoomingBounds
        let fitScale = min(
            zoomBounds.width / imageView.bounds.width,
            zoomBounds.height / imageView.bounds.height
        )
        
        let prevScale = scrollView.zoomScale
        scrollView.minimumZoomScale = fitScale
        scrollView.maximumZoomScale = max(1, fitScale) * imageView.supplementaryZoom
        
        if !hasAlreadyComputedScaleOnce {
            // initial scale after setting image
            hasAlreadyComputedScaleOnce = true
            scrollView.zoomScale = fitScale
        } else {
            // scale computation after an event like rotation; try to preserve the existing scale if possible, bound
            // to the new min.max
            scrollView.zoomScale = min(scrollView.maximumZoomScale, max(scrollView.minimumZoomScale, prevScale))
        }

        updateScrollViewInsets()
    }
    
    private func updateScrollViewInsets() {
        let zoomBounds = zoomingBounds
        scrollView.contentInset.left = max(0, zoomBounds.minX + (zoomBounds.width  - scrollView.contentSize.width)  / 2)
        scrollView.contentInset.top  = max(0, zoomBounds.minY + (zoomBounds.height - scrollView.contentSize.height) / 2)
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

// MARK: Live text
@available(iOS 16.0, *)
extension GalleryImageVC: ImageAnalysisInteractionDelegate {
    static let imageAnalyzer = ImageAnalyzer.isSupported ? ImageAnalyzer() : nil

    private var imageInteraction: ImageAnalysisInteraction? {
        get { return untypedImageInteraction as? ImageAnalysisInteraction }
        set { untypedImageInteraction = newValue }
    }
    
    private func startImageAnalysis() {
        guard let image = imageView.imageForTextAnalysis, let analyzer = GalleryImageVC.imageAnalyzer else {
            self.imageInteraction = nil
            return
        }

        let interaction = ImageAnalysisInteraction()
        interaction.delegate = self
        interaction.preferredInteractionTypes = []
        interaction.analysis = nil
        interaction.supplementaryInterfaceContentInsets.top = 50
        imageView.addInteraction(interaction)
        imageInteraction = interaction

        Task {
            let config = ImageAnalyzer.Configuration([.text, .machineReadableCode])
            do {
                let analysis = try await analyzer.analyze(image, configuration: config)
                interaction.analysis = analysis
                interaction.preferredInteractionTypes = .automatic
            }
            catch {
                print("Coudln't analyze image properly:", error)
            }
        }
    }
    
    private func removeImageInteraction() {
        if let imageInteraction {
            imageView.removeInteraction(imageInteraction)
        }
    }
}

#endif
