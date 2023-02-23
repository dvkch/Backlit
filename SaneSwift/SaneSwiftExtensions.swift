//
//  UIImage+SaneSwift.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import UIKit
import ImageIO
import CommonCrypto

@propertyWrapper
internal struct SaneLocked<Value> {
    private var lock = NSLock()
    private var _stored: Value

    public init(wrappedValue initialValue: Value) {
      _stored = initialValue
    }

    public var wrappedValue: Value {
        get {
            lock.lock()
            defer { lock.unlock() }

            return _stored
        }
        set(newValue) {
            lock.lock()
            defer { lock.unlock() }

            _stored = newValue
        }
    }
}

internal extension String {
    func md5() -> String {
        let length = Int(CC_MD5_DIGEST_LENGTH)
        var digest = [UInt8](repeating: 0, count: length)
        
        if let d = data(using: .utf8) {
            _ = d.withUnsafeBytes {
                CC_MD5($0.baseAddress, CC_LONG(d.count), &digest)
            }
        }
        
        return (0..<length).reduce("") {
            $0 + String(format: "%02x", digest[$1])
        }
    }
}

internal extension Collection {
    func subarray(maxCount: Int) -> Self.SubSequence {
        let max = Swift.min(maxCount, count)
        let maxIndex = index(startIndex, offsetBy: max)
        return self[startIndex..<maxIndex]
    }
}

internal extension Result {
    var error: Error? {
        switch self {
        case .success: return nil
        case .failure(let error): return error
        }
    }
}

internal extension UIImage {
    enum SaneSource {
        case data(Data)
        case file(URL)
        
        var provider: CGDataProvider? {
            switch self {
            case .data(let data):   return CGDataProvider(data: data as CFData)
            case .file(let url):    return CGDataProvider(url: url as CFURL)
            }
        }
    }
    
    static func sy_imageFromSane(source: SaneSource, parameters: ScanParameters) throws -> UIImage {
        return try autoreleasepool {
            guard let provider = source.provider else {
                throw SaneError.noImageData
            }
            
            let colorSpace: CGColorSpace
            let destBitmapInfo: CGBitmapInfo
            let destNumberOfComponents: Int

            switch parameters.currentlyAcquiredChannel {
            case SANE_FRAME_RGB:
                colorSpace = CGColorSpaceCreateDeviceRGB()
                destBitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue)
                destNumberOfComponents = 4
            case SANE_FRAME_GRAY:
                colorSpace = CGColorSpaceCreateDeviceGray()
                destBitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.none.rawValue)
                destNumberOfComponents = 1
            default:
                throw SaneError.unsupportedChannels
            }
            
            guard let sourceImage = CGImage(
                width: parameters.width,
                height: parameters.height,
                bitsPerComponent: parameters.depth,
                bitsPerPixel: parameters.depth * parameters.numberOfChannels,
                bytesPerRow: parameters.bytesPerLine,
                space: colorSpace,
                bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.none.rawValue),
                provider: provider,
                decode: nil,
                shouldInterpolate: false,
                intent: CGColorRenderingIntent.defaultIntent
            ) else {
                throw SaneError.cannotGenerateImage
            }

            guard let context = CGContext(
                data: nil, // let iOS deal with allocating the memory
                width: parameters.width,
                height: parameters.height,
                bitsPerComponent: 8,
                bytesPerRow: parameters.width * destNumberOfComponents,
                space: colorSpace,
                bitmapInfo: destBitmapInfo.rawValue
            ) else {
                throw SaneError.cannotGenerateImage
            }
            
            // LATER: this call seams to leak, at least on Catalyst (macOS 11.3)
            context.draw(sourceImage, in: CGRect(origin: .zero, size: parameters.size))
            
            guard let destCGImage = context.makeImage() else {
                throw SaneError.cannotGenerateImage
            }
            return UIImage(cgImage: destCGImage, scale: 1, orientation: .up)
        }
    }
    
    
    static func sy_imageFromIncompleteSane(data: Data, parameters: ScanParameters) throws -> UIImage {
        let incompleteParams = parameters.incompleteParameters(dataLength: data.count)
        guard incompleteParams.fileSize > 0 else {
            throw SaneError.noImageData
        }
        
        let image = try self.sy_imageFromSane(source: .data(data), parameters: incompleteParams)
        
        UIGraphicsBeginImageContextWithOptions(parameters.size, true, 1.0)
        defer { UIGraphicsEndImageContext() }
        
        UIColor.white.setFill()
        UIBezierPath(rect: CGRect(origin: .zero, size: parameters.size)).fill()
        image.draw(in: CGRect(origin: .zero, size: image.size))
        
        guard let paddedImage = UIGraphicsGetImageFromCurrentImageContext() else {
            throw SaneError.cannotGenerateImage
        }
        return paddedImage
    }
}

/*
// LATER: work on big images
// to stream lower resolutions :
// - https://twitter.com/PDucks32/status/1417553099825238028
// - https://developer.apple.com/documentation/uikit/uiimage/building_high-performance_lists_and_collection_views
+ (void)sy_convertTempImage
{
    SYSaneScanParameters *params = [[SYSaneScanParameters alloc] init];
    params.acquiringLastChannel = YES;
    params.currentlyAcquiredChannel = SANE_FRAME_RGB;
    params.width = 10208;
    params.height = 14032;
    params.bytesPerLine = params.width * 3;
    params.depth = 8;
    
    
    NSString *sourceFileName = [[SYTools documentsPath] stringByAppendingPathComponent:$$("scan.tmp")];
    NSString *destinationFileName = [[SYTools documentsPath] stringByAppendingPathComponent:$$("scan.jpg")];
    
    NSDate *date = [NSDate date];
    
    UIImage *img = [self sy_imageFromRGBData:nil orFileURL:[NSURL fileURLWithPath:sourceFileName] saneParameters:params error:NULL];
    NSData *data = UIImageJPEGRepresentation(img, JPEG_COMP);
    [data writeToFile:destinationFileName atomically:YES];
    
    NSLog($$("-> %@ in %.03lfs"), img, [[NSDate date] timeIntervalSinceDate:date]);
}
*/

