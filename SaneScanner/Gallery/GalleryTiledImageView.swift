//
//  GalleryTiledImageView.swift
//  SaneScanner
//
//  Created by syan on 20/03/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import LRUCache

// Inspired from:
// - https://developer.apple.com/library/content/samplecode/LargeImageDownsizing/Introduction/Intro.html
// - https://github.com/lurado/LDOTiledView

#if !targetEnvironment(macCatalyst)
class GalleryTiledImageView: UIView {
    
    // MARK: Layer
    class TiledLayer: CATiledLayer {
        override class func fadeDuration() -> CFTimeInterval {
            return 0.1
        }
    }
    override class var layerClass: AnyClass {
        return TiledLayer.self
    }
    
    override var layer: TiledLayer {
        get { super.layer as! TiledLayer }
    }
    
    // MARK: Tiling properties
    private let minimumDimensionForTiling: Int = 3000
    private var tileSize: CGSize = .zero {
        didSet {
            layer.tileSize = .init(
                width: tileSize.width * layer.contentsScale,
                height: tileSize.height * layer.contentsScale
            )
            setNeedsDisplay()
        }
    }
    
    // MARK: Properties
    var imageURL: URL? {
        didSet {
            guard imageURL != oldValue else { return }
            if let imageURL, let image = UIImage(contentsOfFile: imageURL.path)?.cgImage {
                if image.width > minimumDimensionForTiling || image.height > minimumDimensionForTiling {
                    // we have an image and we should use tiling because it's big
                    // let's also generate a lower res version of it to quickly show the lowest scale image if possible
                    availableImages = [
                        generateThumbnailIfNeeded(from: image, at: imageURL, maxEdge: minimumDimensionForTiling),
                        image
                    ].removingNils()
                    tileSize = .init(width: 256, height: 256)
                }
                else {
                    // we have an image and it's smaller than (3000, 3000), let's reduce tiling to a minimum
                    // (surprisingly this doesn't disable it...)
                    availableImages = [image]
                    tileSize = .init(width: minimumDimensionForTiling, height: minimumDimensionForTiling)
                }
                imageSize = CGSize(width: image.width, height: image.height)
            }
            else {
                availableImages = []
                tileSize = .init(width: 256, height: 256)
                imageSize = .zero
            }
        }
    }
    private var availableImages: [CGImage] = []

    // MARK: Layout properties
    private(set) var imageSize: CGSize = .zero {
        didSet {
            maximumZoomLevel = [1, Int(imageSize.width / tileSize.width), Int(imageSize.height / tileSize.height)].max()!
            displayedImageSize = .init(
                width: imageSize.width / CGFloat(maximumZoomLevel),
                height: imageSize.height / CGFloat(maximumZoomLevel)
            )
        }
    }
    private(set) var displayedImageSize: CGSize = .zero {
        didSet {
            invalidateIntrinsicContentSize()
            setNeedsDisplay()
        }
    }
    private(set) var maximumZoomLevel: Int = 1 {
        didSet {
            guard maximumZoomLevel >= 1 else {
                self.maximumZoomLevel = 1
                return
            }
            
            layer.levelsOfDetail = maximumZoomLevel
            layer.levelsOfDetailBias = maximumZoomLevel - 1
        }
    }
    var maximumZoomLevelForScrollView: CGFloat {
        // this will make the scrollView at maximum zoom level be precisely 1px on screen to 1px of the image
        // or precisely fit if the image is smaller than the number of pixels on screen
        return max(1, CGFloat(maximumZoomLevel) / layer.contentsScale)
    }
    
    // MARK: Layout
    override var intrinsicContentSize: CGSize {
        return displayedImageSize
    }

    // MARK: Drawing
    override func draw(_ rect: CGRect) {
        // draw background
        super.draw(rect)
        
        // draw tile
        let context = UIGraphicsGetCurrentContext()!
        context.interpolationQuality = .none

        let normalizedContentScale = context.ctm.a / layer.contentsScale
        let currentZoomLevel = Int(log2(CGFloat(max(1, Int(normalizedContentScale))))) + 1
        
        if let tile = bestCroppedTile(in: rect, at: normalizedContentScale) {
            context.saveGState()
            context.concatenate(.init(translationX: rect.origin.x, y: rect.origin.y))
            context.concatenate(.init(a: 1, b: 0, c: 0, d: -1, tx: 0, ty: rect.height))
            context.draw(tile, in: CGRect(origin: .zero, size: rect.size))
            
            // use this to show borders around the tiles
            // UIColor.random.setStroke()
            // context.stroke(CGRect(origin: .zero, size: rect.size), width: 8 / normalizedContentScale)
            
            context.restoreGState()
        }
    }
    
    private func bestCroppedTile(in rect: CGRect, at scale: CGFloat) -> CGImage? {
        let sizeOnScreen = rect.size.applying(.init(scaleX: scale, y: scale))

        let availableCrops = availableImages.map { (image: CGImage) -> (img: CGImage, cropRect: CGRect) in
            let scale = CGFloat(maximumZoomLevel) * CGFloat(image.width) / imageSize.width
            let cropRect = rect
                .applying(.init(scaleX: scale, y: scale))
                .intersection(.init(origin: .zero, size: CGSize(width: image.width, height: image.height)))
            return (image, cropRect)
        }
        
        let cropIndexToUse = availableCrops.firstIndex { (image, cropRect) in
            return cropRect.size.width >= sizeOnScreen.width && cropRect.size.height >= sizeOnScreen.height
        } ?? (availableCrops.count - 1)
        
        guard cropIndexToUse >= 0 else { return nil }
        // print("Using image", cropIndexToUse + 1, "out of", availableImages.count)
        
        let cropToUse = availableCrops[cropIndexToUse]
        return cropToUse.img.cropping(to: cropToUse.cropRect)
    }
    
    private func generateThumbnailIfNeeded(from image: CGImage, at imageURL: URL, maxEdge: Int) -> CGImage? {
        guard image.width > maxEdge, image.height > maxEdge else { return nil }
        
        let thumbFilename = imageURL.deletingPathExtension().lastPathComponent + "-thumb-\(maxEdge).jpg"
        let thumbURL = FileManager.default.temporaryDirectory.appendingPathComponent(thumbFilename, isDirectory: false)
        if let thumb = UIImage(contentsOfFile: thumbURL.path)?.cgImage {
            return thumb
        }
        
        guard let thumb = UIImage.thumbnailForImage(at: imageURL, maxEdgeSize: CGFloat(maxEdge))?.cgImage else {
            return nil
        }
        
        try? UIImage(cgImage: thumb).jpegData(compressionQuality: 0.9)?.write(to: thumbURL)
        return thumb
    }
}
#endif
