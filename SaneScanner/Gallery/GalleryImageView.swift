//
//  GalleryImageView.swift
//  SaneScanner
//
//  Created by syan on 20/03/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import Vision
import SYPictureMetadata

#if !targetEnvironment(macCatalyst)
class GalleryImageView: UIView {
    
    // MARK: Init
    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    private func setup() {
        // TODO: test again 1px for 1px scale

        // TODO: accessibility label using metadata ?
        accessibilityTraits = .image
        isAccessibilityElement = true

        layer.addSublayer(lowResImageLayer)
        
        tiledLayer.tileDelegate = self
        layer.addSublayer(tiledLayer)
    }
    
    // MARK: Layers
    private let lowResImageLayer = GalleryImageViewImageLayer()
    private let tiledLayer = GalleryImageViewTiledLayer()

    // MARK: Tiling properties
    private static let minimumDimensionForTiling: CGFloat = 3000
    static func shouldTile(for imageSize: CGSize) -> Bool {
        return imageSize.width > minimumDimensionForTiling || imageSize.height > minimumDimensionForTiling
    }
    
    // MARK: Properties
    var imageURL: URL? {
        didSet {
            guard imageURL != oldValue else { return }
            guard let imageURL, let image = UIImage(contentsOfFile: imageURL.path) else {
                tiledLayer.image = nil
                lowResImageLayer.image = nil
                displayedImageSize = .zero
                return
            }
            
            // if we have an image that seems big we will automatically generate a low res version of it, who's longest
            // edge will be 3000px, and enable tiling. This will allow a quicker previewing of the most zoomed out viewing
            // of the image while still allowing showing the full resolution when zooming
            if type(of: self).shouldTile(for: image.size), let lowRes = generateLowResIfNeeded(from: image, at: imageURL) {
                tiledLayer.image = image.cgImage
                lowResImageLayer.image = lowRes.cgImage
                displayedImageSize = .init(
                    width: image.size.width / CGFloat(tiledLayer.maximumZoomLevel),
                    height: image.size.height / CGFloat(tiledLayer.maximumZoomLevel)
                )
            }
            // if the image is small enough not we disable tiling altogether
            else {
                tiledLayer.image = nil
                lowResImageLayer.image = image.cgImage
                displayedImageSize = image.size
            }
        }
    }
    
    var imageForTextAnalysis: UIImage? {
        guard let cgImage = lowResImageLayer.image else { return nil }
        return UIImage(cgImage: cgImage)
    }

    // MARK: Layout properties
    private(set) var displayedImageSize: CGSize = .zero {
        didSet {
            invalidateIntrinsicContentSize()
            setNeedsDisplay()
        }
    }
    var supplementaryZoom: CGFloat {
        // this will make the scrollView at maximum zoom level be precisely 1px on screen to 1px of the image
        // or precisely fit if the image is smaller than the number of pixels on screen
        return max(1, CGFloat(tiledLayer.maximumZoomLevel) / layer.contentsScale)
    }
    
    // MARK: Layout
    override var intrinsicContentSize: CGSize {
        return displayedImageSize
    }

    // MARK: Drawing
    override func layoutSublayers(of layer: CALayer) {
        super.layoutSublayers(of: layer)
        lowResImageLayer.frame = layer.bounds
        tiledLayer.frame = layer.bounds
    }
    
    private func generateLowResIfNeeded(from image: UIImage, at imageURL: URL, maxEdge: CGFloat = minimumDimensionForTiling) -> UIImage? {
        guard image.size.width > maxEdge || image.size.height > maxEdge else { return nil }
        
        let thumbFilename = imageURL.deletingPathExtension().lastPathComponent + "-lowres-\(maxEdge).jpg"
        let thumbURL = FileManager.default.temporaryDirectory.appendingPathComponent(thumbFilename, isDirectory: false)
        if let thumb = UIImage(contentsOfFile: thumbURL.path) {
            return thumb
        }
        
        guard let thumb = UIImage.thumbnailForImage(at: imageURL, maxEdgeSize: maxEdge) else {
            return nil
        }
        
        try? thumb.jpegData(compressionQuality: 0.9)?.write(to: thumbURL)
        return thumb
    }
}

extension GalleryImageView: GalleryImageViewTiledLayerDelegate {
    func galleryImageViewTiledLayer(_ layer: GalleryImageViewTiledLayer, shouldDraw image: CGImage, in rect: CGRect, scale: CGFloat) -> Bool {
        guard let lowResImage = lowResImageLayer.image else { return true }
        let adaptedScale = CGFloat(lowResImage.width) / CGFloat(image.width) * CGFloat(tiledLayer.maximumZoomLevel)
        let cropRect = rect
            .applying(.init(scaleX: adaptedScale, y: adaptedScale))
            .intersection(.init(origin: .zero, size: CGSize(width: lowResImage.width, height: lowResImage.height)))
        
        let displaySize = rect.size.applying(.init(scaleX: scale, y: scale))
        return displaySize.width > cropRect.width || displaySize.height > cropRect.height
    }
}
#endif
