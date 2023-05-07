//
//  UIImage+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

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

extension CGImage {
    var grayscale: CGImage? {
        guard let filter = CIFilter(name: "CIPhotoEffectNoir") else { return nil }
        filter.setValue(CIImage(cgImage: self), forKey: kCIInputImageKey)

        guard let output = filter.outputImage else { return nil }
        return CIContext(options: nil).createCGImage(output, from: output.extent)
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
