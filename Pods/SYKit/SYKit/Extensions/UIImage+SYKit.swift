//
//  UIImage+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import ImageIO

// Informations
@objc public extension UIImage {
    
    @objc(sy_hasAlpha)
    var hasAlpha: Bool {
        // https://github.com/mbcharbonneau/UIImage-Categories/blob/master/UIImage%2BAlpha.m
        guard let info = self.cgImage?.alphaInfo else { return false }
        return info == .first || info == .last || info == .alphaOnly || info == .premultipliedFirst || info == .premultipliedLast
    }
    
    @objc(sy_sizeOfImageAtURL:)
    static func sizeOfImageOBJC(at url: URL) -> CGSize {
        return self.sizeOfImage(at: url) ?? .zero
    }
    
    @nonobjc static func sizeOfImage(at url: URL) -> CGSize? {
        // http://oleb.net/blog/2011/09/accessing-image-properties-without-loading-the-image-into-memory/
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil) else { return nil }
        
        let options = [kCGImageSourceShouldCache: kCFBooleanFalse] as CFDictionary
        guard let properties = CGImageSourceCopyPropertiesAtIndex(source, 0, options) as? [String: AnyObject] else { return nil }
        
        let width  = properties[kCGImagePropertyPixelWidth  as String] as? NSNumber
        let height = properties[kCGImagePropertyPixelHeight as String] as? NSNumber
        
        if let width = width, let height = height {
            return CGSize(width: width.doubleValue, height: height.doubleValue)
        }
        return nil
    }

    @objc enum ThumbnailOptions: Int {
        case alwaysCreate
        case createIfAbsent
        case dontCreate
    }
    @objc(sy_thumbnailForImageAtURL:maxEdgeSize:options:)
    static func thumbnailForImage(at url: URL, maxEdgeSize: CGFloat, options: ThumbnailOptions) -> UIImage? {
        // http://stackoverflow.com/a/5860390/1439489
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil) else { return nil }
        
        var cgOptions: [CFString: Any] = [
            kCGImageSourceCreateThumbnailWithTransform: true,
            kCGImageSourceThumbnailMaxPixelSize: maxEdgeSize as NSNumber,
        ]
        
        switch options {
        case .alwaysCreate:
            cgOptions[kCGImageSourceCreateThumbnailFromImageAlways] = true
        case .createIfAbsent:
            cgOptions[kCGImageSourceCreateThumbnailFromImageIfAbsent] = true
        case .dontCreate:
            cgOptions[kCGImageSourceCreateThumbnailFromImageIfAbsent] = false
        }
        
        guard let thumb = CGImageSourceCreateThumbnailAtIndex(source, 0, cgOptions as CFDictionary) else { return nil }
        return UIImage(cgImage: thumb)
    }
}

// Creation
@objc public extension UIImage {
    
    @objc(sy_imageWithColor:)
    convenience init?(color: UIColor?) {
        self.init(color: color, size: CGSize(width: 1, height: 1), cornerRadius: 0)
    }
    
    @objc(sy_imageWithColor:size:cornerRadius:)
    convenience init?(color: UIColor?, size: CGSize = CGSize(width: 1, height: 1), cornerRadius: CGFloat = 0) {
        guard let color = color else { return nil }
        
        let rect = CGRect(origin: .zero, size: size)
        
        UIGraphicsBeginImageContextWithOptions(rect.size, cornerRadius == 0 && color.alpha == 1, UIScreen.main.scale)
        defer { UIGraphicsEndImageContext() }

        color.setFill()
        UIBezierPath(roundedRect: rect, cornerRadius: cornerRadius).fill()
        
        guard let cgImage = UIGraphicsGetImageFromCurrentImageContext()?.cgImage else { return nil }
        self.init(cgImage: cgImage)
    }
}

// Modifications
@objc public extension UIImage {

    @objc(sy_addingPaddingWithTop:left:right:bottom:)
    func addingPadding(top: CGFloat = 0, left: CGFloat = 0, right: CGFloat = 0, bottom: CGFloat = 0) -> UIImage? {
        var size = self.size
        size.width += left + right
        size.height += top + bottom
        
        UIGraphicsBeginImageContextWithOptions(size, false, scale)
        defer { UIGraphicsEndImageContext() }
        draw(at: CGPoint(x: left, y: top))
        return UIGraphicsGetImageFromCurrentImageContext()
    }
    
    @objc(sy_resizingToSize:interpolation:)
    func resizing(to size: CGSize, interpolation: CGInterpolationQuality = .high) -> UIImage? {
        // http://www.lukaszielinski.de/blog/posts/2014/01/21/ios-how-to-resize-and-rotate-uiimages-in-a-thread-safe-fashion/
        guard let cgImage = self.cgImage, let colorSpace = cgImage.colorSpace else { return nil }
        
        let targetWidth  = size_t(size.width  * scale)
        let targetHeight = size_t(size.height * scale)
    
        guard let context = CGContext(
            data: nil,
            width: targetWidth,
            height: targetHeight,
            bitsPerComponent: cgImage.bitsPerComponent,
            bytesPerRow: cgImage.bitsPerPixel * targetWidth,
            space: colorSpace,
            bitmapInfo: cgImage.alphaInfo.rawValue
            ) else { return nil }
    
        context.interpolationQuality = interpolation
        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: targetWidth, height: targetHeight))
        
        guard let newCGImage = context.makeImage() else { return nil }
        return UIImage(cgImage: newCGImage, scale: scale, orientation: imageOrientation)
    }

    @objc(sy_resizingWidthTo:interpolation:)
    func resizingWidth(to width: CGFloat, interpolation: CGInterpolationQuality = .high) -> UIImage? {
        return self.resizing(to: CGSize(width: width, height: size.height * width / size.width), interpolation: interpolation)
    }
    
    @objc(sy_resizingHeightTo:interpolation:)
    func resizingHeight(to height: CGFloat, interpolation: CGInterpolationQuality = .high) -> UIImage? {
        return self.resizing(to: CGSize(width: size.width * height / size.height, height: height), interpolation: interpolation)
    }
    
    @objc(sy_resizingLongestEdgeTo:interpolation:)
    func resizingLongestEdge(to size: CGFloat, interpolation: CGInterpolationQuality = .high) -> UIImage? {
        if self.size.width > self.size.height {
            return resizingWidth(to: size, interpolation: interpolation)
        } else {
            return resizingHeight(to: size, interpolation: interpolation)
        }
    }
    
    @objc(sy_maskingWithColor:)
    func masking(with color: UIColor) -> UIImage? {
        guard let cgImage = self.cgImage else { return nil }
        let rect = CGRect(origin: .zero, size: size)
        
        UIGraphicsBeginImageContextWithOptions(rect.size, false, scale)
        defer { UIGraphicsEndImageContext() }
        
        guard let context = UIGraphicsGetCurrentContext() else { return nil }
        context.scaleBy(x: 1, y: -1)
        context.translateBy(x: 0, y: -rect.size.height)
        context.clip(to: rect, mask: cgImage)
        context.setFillColor(color.cgColor)
        context.fill(rect)
        return UIGraphicsGetImageFromCurrentImageContext()
    }
    
    @objc(sy_rotatingToDegrees:)
    func rotating(degrees: CGFloat) -> UIImage? {
        // http://www.lukaszielinski.de/blog/posts/2014/01/21/ios-how-to-resize-and-rotate-uiimages-in-a-thread-safe-fashion/
        guard let cgImage = self.cgImage, let colorSpace = cgImage.colorSpace else { return nil }
        
        let radians = degrees * CGFloat.pi / 180
        
        let imageRect = CGRect(x: 0, y: 0, width: size.width * scale, height: size.height * scale)
        let rotatedRect = imageRect.applying(CGAffineTransform(rotationAngle: radians))
        
        guard let context = CGContext(
            data: nil,
            width: Int(rotatedRect.width),
            height: Int(rotatedRect.height),
            bitsPerComponent: cgImage.bitsPerComponent,
            bytesPerRow: cgImage.bitsPerPixel * Int(rotatedRect.width),
            space: colorSpace,
            bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue
            ) else { return nil }

        context.setShouldAntialias(true)
        context.setAllowsAntialiasing(true)
        context.interpolationQuality = .high
        
        context.translateBy(x: rotatedRect.size.width * 0.5, y: rotatedRect.size.height * 0.5)
        context.rotate(by: radians)
        context.draw(cgImage, in: CGRect(
            x: -size.width * 0.5 * scale,
            y: -size.height * 0.5 * scale,
            width: size.width * scale,
            height: size.height * scale
        ))
        
        guard let newCGImage = context.makeImage() else { return nil }
        return UIImage(cgImage: newCGImage, scale: scale, orientation: imageOrientation)
    }

    @objc(sy_roundedClip)
    var roundedClip: UIImage? {
        let size = min(self.size.width, self.size.height)
        
        UIGraphicsBeginImageContextWithOptions(CGSize(width: size, height: size), false, scale)
        defer { UIGraphicsEndImageContext() }
        
        UIBezierPath(ovalIn: CGRect(x: 0, y: 0, width: size, height: size)).addClip()
        draw(at: CGPoint(x: (size - self.size.width) / 2, y: (size - self.size.height) / 2))
        
        return UIGraphicsGetImageFromCurrentImageContext()
    }
    
    @objc(sy_applyingGreyscaleFilter:)
    func greyscale(using filter: CIImage.GreyscaleFilter) -> UIImage? {
        guard let ciImage = CIImage(image: self)?.greyscale(using: filter) else { return nil }
        guard let cgImage = CIContext(options: nil).createCGImage(ciImage, from: ciImage.extent) else { return nil }
        return UIImage(cgImage: cgImage)
    }
}

public extension CGImage {
    func greyscale(using filter: CIImage.GreyscaleFilter) -> CGImage? {
        guard let ciImage = CIImage(cgImage: self).greyscale(using: filter) else { return nil }
        return CIContext(options: nil).createCGImage(ciImage, from: ciImage.extent)
    }
}

public extension CIImage {
    @objc(CIImageGreyscaleFilter)
    enum GreyscaleFilter: Int {
        case noir, mono, tonal
        public var filterName: String {
            switch self {
            case .noir:     return "CIPhotoEffectNoir"
            case .mono:     return "CIPhotoEffectMono"
            case .tonal:    return "CIPhotoEffectTonal"
            }
        }
    }
    
    func greyscale(using filter: GreyscaleFilter) -> CIImage? {
        guard let filter = CIFilter(name: filter.filterName) else { return nil }
        filter.setValue(self, forKey: kCIInputImageKey)
        return filter.outputImage
    }
}
