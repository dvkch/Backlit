//
//  UIImage+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

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
    
    func scanPngData() -> Data? {
        // TODO: check for leaks
        guard let cgImage else { return pngData() }
        
        // images created with CoreGraphics in Gray with 1bit per pixel don't get saved properly by `.pngData()`
        // so we revert to the proper methods in CoreGraphics to generate our PNG data
        let outputData = CFDataCreateMutable(nil, 0)!
        guard let destination = CGImageDestinationCreateWithData(outputData, kUTTypePNG as CFString, 1, nil) else {
            return pngData()
        }
        CGImageDestinationAddImage(destination, cgImage, nil)
        guard CGImageDestinationFinalize(destination) else {
            return pngData()
        }
        return outputData as Data
    }
}
