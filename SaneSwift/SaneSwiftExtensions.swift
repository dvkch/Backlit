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

internal extension Numeric where Self: Comparable {
    func clamped(min: Self, max: Self) -> Self {
        return Swift.max(min, Swift.min(max, self))
    }
}

internal extension Collection {
    func subarray(maxCount: Int) -> Self.SubSequence {
        let max = Swift.min(maxCount, count)
        let maxIndex = index(startIndex, offsetBy: max)
        return self[startIndex..<maxIndex]
    }
}

internal extension Collection where Element: Comparable {
    func closest(to value: Element) -> Element? {
        let sorted = self.sorted()
        return sorted.first(where: { $0 >= value }) ?? sorted.last(where: { $0 <= value })
    }
}

internal extension Result {
    var value: Success? {
        switch self {
        case .success(let value): return value
        case .failure: return nil
        }
    }
    var error: Failure? {
        switch self {
        case .success: return nil
        case .failure(let error): return error
        }
    }
}

public protocol WithID<ID> {
    associatedtype ID : Hashable
    var id: Self.ID { get }
}

internal extension Array where Element: WithID {
    func find(_ id: Element.ID) -> Element? {
        return first(where: { $0.id == id })
    }
}

internal extension Array where Element == String {
    func saneJoined() -> String {
        return map { (element: String) -> String in
            if !element.contains(":") {
                return element
            }
            if element.hasPrefix("[") && element.hasSuffix("]") {
                return element
            }
            return "[\(element)]"
        }.joined(separator: ":")
    }
}

internal extension String {
    func saneSplit() -> [String] {
        var result: [String] = []
        var currentComponent = ""
        var bracketsLevel = 0
        
        for char in self {
            if char == ":" && bracketsLevel == 0 {
                result.append(currentComponent)
                currentComponent = ""
            }
            else {
                currentComponent.append(char)
                if char == "[" {
                    bracketsLevel += 1
                }
                else if char == "]" {
                    bracketsLevel -= 1
                }
            }
        }
        result.append(currentComponent)
        return result
    }
}

internal extension UInt8 {
    var bits: (UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8) {
        return (
            self & 0b10000000 >> 7,
            self & 0b01000000 >> 6,
            self & 0b00100000 >> 5,
            self & 0b00010000 >> 4,
            self & 0b00001000 >> 3,
            self & 0b00000100 >> 2,
            self & 0b00000010 >> 1,
            self & 0b00000001 >> 0
        )
    }

    static func fromBits(b7: UInt8, b6: UInt8, b5: UInt8, b4: UInt8, b3: UInt8, b2: UInt8, b1: UInt8, b0: UInt8) -> UInt8 {
        // need to split to make Swift compile the expression...
        var value: UInt8 = (b0 << 0) + (b1 << 1) + (b2 << 2) + (b3 << 3)
        value +=           (b4 << 4) + (b5 << 5) + (b6 << 6) + (b7 << 7)
        return value
    }
}

internal func unpack1BitPixels(r: UInt8, g: UInt8, b: UInt8) -> (byte0: UInt8, byte1: UInt8, byte2: UInt8) {
    let rBits = r.bits
    let gBits = g.bits
    let bBits = b.bits
    return (
        byte0: .fromBits(b7: rBits.0, b6: gBits.0, b5: bBits.0, b4: rBits.1, b3: gBits.1, b2: bBits.1, b1: rBits.2, b0: gBits.2),
        byte1: .fromBits(b7: bBits.2, b6: rBits.3, b5: gBits.3, b4: bBits.3, b3: rBits.4, b2: gBits.4, b1: bBits.4, b0: rBits.5),
        byte2: .fromBits(b7: gBits.5, b6: bBits.5, b5: rBits.6, b4: gBits.6, b3: bBits.6, b2: rBits.7, b1: gBits.7, b0: bBits.7)
    )
}

internal extension Data {
    // TODO: make it work with 3 pass
    var unpackingSingleBitColorPixels: Data {
        // super slow in Debug, works quite nicely in Release (0.3s for 28MB on M1 Pro)
        let d = Date()
        var unpackedData = self + Data(repeating: 0, count: 3 - (count % 3))
        for i in stride(from: 0, to: unpackedData.count, by: 3) {
            var r = unpackedData[i]
            var g = unpackedData[i + 1]
            var b = unpackedData[i + 2]
            unpackPixels(&r, &g, &b)
            unpackedData[i    ] = r
            unpackedData[i + 1] = g
            unpackedData[i + 2] = b
        }
        print("\(Date().timeIntervalSince(d))s for \(unpackedData.count) bytes")
        return unpackedData
    }
}

internal extension UIImage {
    // Nota: if the source image is 1 bit Grayscale (monochrome), the method `pngData()` on the output UIImage will
    // produce an invalid file (at least on macOS 13.2.1), but using CGImageDestinationCreateWithData with kUTTypePNG
    // will produce a valid PNG file.
    static func sy_imageFromSane(data: Data, parameters: ScanParameters) throws -> UIImage {
        // TODO: release pool necessary ?
        return try autoreleasepool {
            let colorSpace: CGColorSpace
            let shouldUnpackPixels: Bool

            switch parameters.currentlyAcquiredFrame {
            case SANE_FRAME_RGB:
                colorSpace = CGColorSpaceCreateDeviceRGB()
                shouldUnpackPixels = parameters.depth == 1
            case SANE_FRAME_GRAY:
                colorSpace = CGColorSpaceCreateDeviceGray()
                shouldUnpackPixels = false
            case SANE_FRAME_RED, SANE_FRAME_GREEN, SANE_FRAME_BLUE:
                colorSpace = CGColorSpaceCreateDeviceRGB()
                shouldUnpackPixels = parameters.depth == 1
            default:
                throw SaneError.unsupportedChannels
            }

            let imageData = shouldUnpackPixels ? data.unpackingSingleBitColorPixels : data
            guard let provider = CGDataProvider(data: imageData as CFData) else {
                throw SaneError.noImageData
            }

            var bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.none.rawValue)
            if parameters.depth >= 16 {
                bitmapInfo.insert(CGBitmapInfo(rawValue: CGImageByteOrderInfo.order16Little.rawValue))
            }
            
            guard let cgImage = CGImage(
                width: parameters.width,
                height: parameters.height,
                bitsPerComponent: parameters.depth,
                bitsPerPixel: parameters.depth * parameters.numberOfChannels * parameters.expectedFramesCount,
                bytesPerRow: parameters.bytesPerLine * parameters.expectedFramesCount,
                space: colorSpace,
                bitmapInfo: bitmapInfo,
                provider: provider,
                decode: nil,
                shouldInterpolate: false,
                intent: .defaultIntent
            ) else {
                throw SaneError.cannotGenerateImage
            }

            return UIImage(cgImage: cgImage)
        }
    }

    static func sy_imageFromIncompleteSane(data: Data, parameters: ScanParameters) throws -> UIImage {
        let size = (parameters.fileSize * parameters.expectedFramesCount) - data.count
        let completedData = data + Data(repeating: UInt8.max, count: max(0, size))
        return try self.sy_imageFromSane(data: completedData, parameters: parameters)
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

