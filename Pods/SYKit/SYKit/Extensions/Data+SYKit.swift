//
//  Data+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public extension Data {
    // http://stackoverflow.com/questions/6824357/png-validation-on-ios
    var isValidPNGImageData: Bool {
        guard count >= 12 else { return false }
        
        return withUnsafeBytes { (pointer) -> Bool in
            let bytes = pointer.bindMemory(to: UInt8.self)
            return (bytes[0] == 0x89 && // PNG
                bytes[1] == 0x50 &&
                bytes[2] == 0x4e &&
                bytes[3] == 0x47 &&
                bytes[4] == 0x0d &&
                bytes[5] == 0x0a &&
                bytes[6] == 0x1a &&
                bytes[7] == 0x0a &&
                
                bytes[count - 12] == 0x00 && // IEND
                bytes[count - 11] == 0x00 &&
                bytes[count - 10] == 0x00 &&
                bytes[count -  9] == 0x00 &&
                bytes[count -  8] == 0x49 &&
                bytes[count -  7] == 0x45 &&
                bytes[count -  6] == 0x4e &&
                bytes[count -  5] == 0x44 &&
                bytes[count -  4] == 0xae &&
                bytes[count -  3] == 0x42 &&
                bytes[count -  2] == 0x60 &&
                bytes[count -  1] == 0x82)
        }
    }
    
    // http://stackoverflow.com/questions/9265343/imageio-error-jpeg-corrupt-jpeg-data-premature-end-of-data-segment-iphone/9990940#9990940
    var isValidJPEGImageData: Bool {
        guard count >= 4 else { return false }
        
        return withUnsafeBytes { (pointer) -> Bool in
            let bytes = pointer.bindMemory(to: UInt8.self)
            return (bytes[0] == 0xFF &&
                bytes[1] == 0xD8 &&
                
                bytes[count - 2] == 0xFF &&
                bytes[count - 1] == 0xD9)
        }
    }
}

public extension NSData {
    @objc(sy_isValidPNGImageData)
    var isValidPNGImageData: Bool {
        return (self as Data).isValidPNGImageData
    }
    
    @objc(sy_isValidJPEGImageData)
    var isValidJPEGImageData: Bool {
        return (self as Data).isValidJPEGImageData
    }
}

