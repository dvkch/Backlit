//
//  UIImage+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import MobileCoreServices
import SYPictureMetadata

extension UIImage {
    enum Icon: String {
        case open       = "folder"                  // 23x17
        case share      = "square.and.arrow.up"     // 19x21
        case save       = "square.and.arrow.down"   // 19x20
        case checkmark  = "checkmark"               // 18x16
        case auto       = "wand.and.stars.inverse"  // 21x19
        case edit       = "pencil"                  // 17x15
        case copy       = "doc.on.doc"              // 21x23
        case delete     = "trash"                   // 19x21
        case close      = "xmark"                   // 17x15
        case settings   = "gear"                    // 22x21
        case pdf        = "pdf"                     // 19x19 (home made)
        case network    = "network"                 // 20x19
        case scanner    = "scanner"                 // 24x17
        case scannerSmall = "scanner-shortcut"
        case up         = "chevron.up"              // 19x10
        case down       = "chevron.down"            // 19x10
        case left       = "chevron.left"            // 13x17
        case right      = "chevron.right"           // 13x17
        case refresh    = "arrow.clockwise"         // 18x20

        var availableVariantsCount: Int {
            switch self {
            case .right, .down: return 12
            case .close:        return  4
            default:            return  1
            }
        }
        
        func assetName(variant: Int?) -> String {
            guard availableVariantsCount > 1 else { return rawValue }
            let boundedVariant: Int
            if let variant {
                boundedVariant = variant % availableVariantsCount
            }
            else {
                boundedVariant = Int.random(in: 0..<availableVariantsCount)
            }
            return "\(rawValue).\(boundedVariant)"
        }
    }

    static func icon(_ icon: Icon, variant: Int? = 0, useSystem: Bool = false) -> UIImage? {
        if useSystem, #available(iOS 13.0, *) {
            return UIImage(systemName: icon.rawValue)
        }

        // could try using:
        // let config = UIImage.SymbolConfiguration(font: UIFontMetrics.default.scaledFont(for: .systemFont(ofSize: 10)))
        return UIImage(named: icon.assetName(variant: variant))
    }
}

extension UIImage {
    static func testImage(size: CGFloat) -> UIImage? {
        let letter = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789".randomElement() ?? "A"
        let label = UILabel()
        label.font = UIFont.boldSystemFont(ofSize: size / 2)
        label.textAlignment = .center
        label.text = String(letter)
        label.textColor = .white
        label.backgroundColor = .clear
        label.frame = CGRect(x: 0, y: 0, width: size, height: size)
        
        return .screenshottingContext(size: CGSize(width: size, height: size), scale: 1) {
            UIColor.random.setFill()
            UIBezierPath(rect: label.bounds).fill()
            
            label.draw(label.bounds)
        }
    }
    
    static func screenshottingContext(size: CGSize, scale: CGFloat = UIScreen.main.scale, _ block: () -> ()) -> UIImage? {
        UIGraphicsBeginImageContextWithOptions(size, false, scale)
        defer { UIGraphicsEndImageContext() }
        block()
        return UIGraphicsGetImageFromCurrentImageContext()
    }
    
    var estimatedMemoryFootprint: Int {
        if let data = cgImage?.dataProvider?.data {
            return CFDataGetLength(data)
        }
        return Int(size.width) * Int(size.height) * (cgImage?.bitsPerPixel ?? 32) / 8
    }
}

// MARK: Compression methods
extension UIImage {
    enum ImageFormat {
        case png
        case jpeg(quality: CGFloat)
        case heic(quality: CGFloat)
        
        var utType: CFString {
            switch self {
            case .png:  return kUTTypePNG as CFString
            case .jpeg: return kUTTypeJPEG as CFString
            case .heic: return "public.heic" as CFString
            }
        }
        
        var fileExtension: String {
            switch self {
            case .png:  return "png"
            case .jpeg: return "jpg"
            case .heic: return "heic"
            }
        }
        
        func systemEncode(image: UIImage, metadata: [String: Any]) -> Data? {
            // this is the default way to encode. unfortunately it is twice as slow since it compresses
            // once to NSData, then again to add the metadata, since UIImage helper methods don't allow
            // to pass it directly. It also fucks up monochrome images.
            // we use it only as a fallback if manually using `CGImageDestination` doesn't work
            var data: Data?
            switch self {
            case .png:                  data = image.pngData()
            case .jpeg(let quality):    data = image.jpegData(compressionQuality: quality)
            case .heic:                 data = nil
            }
            guard let data else { return nil }
            return (try? SYMetadata.apply(metadata: metadata, to: data)) ?? data
        }
    }
    
    // images created with CoreGraphics in Gray with 1bit per pixel don't get saved properly by `.pngData()`
    // so we revert to the proper methods in CoreGraphics to generate our PNG data.
    // we also use it for JPEG encoding, as using SYMetadata methods would rencode the already encoded UIImage
    // and we'd end up with 2x the total encoding time just to add the metadata
    func scanData(format: ImageFormat, metadata: [String: Any]) -> Data? {
        guard let cgImage else {
            return format.systemEncode(image: self, metadata: metadata)
        }
        
        let outputData = CFDataCreateMutable(nil, 0)!
        var options = metadata
        options[kCGImageDestinationEmbedThumbnail as String] = kCFBooleanTrue
        options[kCGImageSourceCreateThumbnailFromImageAlways as String] = kCFBooleanTrue
        options[kCGImageSourceCreateThumbnailWithTransform as String] = kCFBooleanTrue
        options[kCGImageSourceThumbnailMaxPixelSize as String] = 256

        switch format {
        case .jpeg(let quality), .heic(let quality):
            options[kCGImageDestinationLossyCompressionQuality as String] = quality
        case .png:
            break
        }

        guard let destination = CGImageDestinationCreateWithData(outputData, format.utType, 1, options as NSDictionary) else {
            return format.systemEncode(image: self, metadata: metadata)
        }

        CGImageDestinationSetProperties(destination, options as CFDictionary)
        CGImageDestinationAddImage(destination, cgImage, options as CFDictionary)
        guard CGImageDestinationFinalize(destination) else {
            return format.systemEncode(image: self, metadata: metadata)
        }
        return outputData as Data
    }
}
