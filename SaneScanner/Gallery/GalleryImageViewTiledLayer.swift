//
//  GalleryImageViewTiledLayer.swift
//  SaneScanner
//
//  Created by syan on 03/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

// Inspired from:
// - https://developer.apple.com/library/content/samplecode/LargeImageDownsizing/Introduction/Intro.html
// - https://github.com/lurado/LDOTiledView
// - https://stackoverflow.com/a/12001270/1439489

#if !targetEnvironment(macCatalyst)
protocol GalleryImageViewTiledLayerDelegate: NSObjectProtocol {
    func galleryImageViewTiledLayer(_ layer: GalleryImageViewTiledLayer, shouldDraw image: CGImage, in rect: CGRect, scale: CGFloat) -> Bool
}

class GalleryImageViewTiledLayer: CATiledLayer {
    
    // MARK: Inherited properties
    override class func fadeDuration() -> CFTimeInterval {
        return 0.1
    }

    // MARK: Properties
    var image: CGImage? {
        didSet {
            tileSize = .init(width: 256 * contentsScale, height: 256 * contentsScale)
            imageSize = CGSize(width: image?.width ?? 0, height: image?.height ?? 0)
            setNeedsDisplay()
        }
    }

    // MARK: Layout properties
    weak var tileDelegate: GalleryImageViewTiledLayerDelegate?
    private var imageSize: CGSize = .zero {
        didSet {
            maximumZoomLevel = [1, Int(imageSize.width / tileSize.width), Int(imageSize.height / tileSize.height)].max()!
        }
    }
    private(set) var maximumZoomLevel: Int = 1 {
        didSet {
            guard maximumZoomLevel >= 1 else {
                self.maximumZoomLevel = 1
                return
            }
            
            levelsOfDetail = maximumZoomLevel
            levelsOfDetailBias = maximumZoomLevel - 1
        }
    }
    
    // MARK: Drawing
    override func draw(in context: CGContext) {
        super.draw(in: context)
        context.interpolationQuality = .default
        
        // draw tile
        let rect = context.boundingBoxOfClipPath
        let normalizedScale = context.ctm.a / contentsScale
        // let currentZoomLevel = Int(log2(CGFloat(max(1, Int(normalizedScale))))) + 1

        guard let image else { return }
        let shouldDraw = tileDelegate?.galleryImageViewTiledLayer(self, shouldDraw: image, in: rect, scale: normalizedScale) ?? true
        guard shouldDraw else {
            // parent layer told us to ignore drawing, because it is already displaying a lowRes version of the image
            // this is high quality enough to not bother decoding and loading the full res
            return
        }

        context.saveGState();
        {
            context.concatenate(.init(translationX: rect.origin.x, y: rect.origin.y))
            context.concatenate(.init(a: 1, b: 0, c: 0, d: -1, tx: 0, ty: rect.height))
            if let tile = generateTile(in: rect, at: normalizedScale) {
                context.draw(tile, in: CGRect(origin: .zero, size: rect.size))
            }

            // use this to show borders around the tiles
            // context.setStrokeColor(UIColor.random.cgColor)
            // context.stroke(CGRect(origin: .zero, size: rect.size), width: 8 / normalizedScale)
        }()
        context.restoreGState()
    }
    
    private func generateTile(in rect: CGRect, at scale: CGFloat) -> CGImage? {
        let cropRect = rect
            .applying(.init(scaleX: CGFloat(maximumZoomLevel), y: CGFloat(maximumZoomLevel)))
            .intersection(.init(origin: .zero, size: imageSize))
        
        return image?.cropping(to: cropRect)
    }
}
#endif
