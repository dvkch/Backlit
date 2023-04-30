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
import MobileCoreServices

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

internal extension Data {
    // NOTA: this doesn't always work with three-pass. suprisingly two scenarios occur:
    // - the scanner sends G, G, B frames, and it works properly
    // - the scanner sends R, G, B frames, and it doesn't work
    // tested on macOS 13.3, test backend 1.1.1
    var unpackingSingleBitColorPixels: Data {
        // bit slow in Debug, works quite nicely in Release (6.5s for 2.5GB on M1 Pro)
        var unpackedData = self + Data(repeating: 0, count: 3 - (count % 3))
        let indexes = stride(from: 0, to: unpackedData.count, by: 3)

        unpackedData.withUnsafeMutableBytes { (content: UnsafeMutableRawBufferPointer) -> Void in
            let bytes = content.assumingMemoryBound(to: UInt8.self).baseAddress!
            var r: UInt8 = 0
            var g: UInt8 = 0
            var b: UInt8 = 0
            for i in indexes {
                r = bytes.advanced(by: i).pointee
                g = bytes.advanced(by: i + 1).pointee
                b = bytes.advanced(by: i + 2).pointee
                unpackPixels(&r, &g, &b)
                bytes.advanced(by: i).pointee = r
                bytes.advanced(by: i + 1).pointee = g
                bytes.advanced(by: i + 2).pointee = b
            }
        }

        return unpackedData
    }
}

internal extension UIImage {
    // Nota: if the source image is 1 bit Grayscale (monochrome), the method `pngData()` on the output UIImage will
    // produce an invalid file (at least on macOS 13.2.1), but using CGImageDestinationCreateWithData with kUTTypePNG
    // will produce a valid PNG file.
    static func imageFromSane(data: Data, parameters: ScanParameters) throws -> UIImage {
        // autorelease pool needed. during preview a lot of intermediary images are created, if we were
        // to remove the autorelease pool they wouldn't be released until the preview if finished. tested
        // up until iOS 16.3 using Instruments
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

    static func imageFromIncompleteSane(data: Data, parameters: ScanParameters) throws -> UIImage {
        let size = (parameters.fileSize * parameters.expectedFramesCount) - data.count
        let completedData = data + Data(repeating: UInt8.max, count: max(0, size))
        return try self.imageFromSane(data: completedData, parameters: parameters)
    }
}

// MARK: Compression methods
public extension UIImage {
    enum ImageFormat {
        case png
        case jpeg(quality: CGFloat)
        case heic(quality: CGFloat)
        
        public var utType: CFString {
            switch self {
            case .png:  return kUTTypePNG as CFString
            case .jpeg: return kUTTypeJPEG as CFString
            case .heic: return "public.heic" as CFString
            }
        }
        
        public var fileExtension: String {
            switch self {
            case .png:  return "png"
            case .jpeg: return "jpg"
            case .heic: return "heic"
            }
        }
    }
    
    // images created with CoreGraphics in Gray with 1bit per pixel don't get saved properly by `.pngData()`
    // so we revert to the proper methods in CoreGraphics to generate our PNG data.
    // we also use it for JPEG encoding, as using SYMetadata methods would rencode the already encoded UIImage
    // and we'd end up with 2x the total encoding time just to add the metadata
    func scanData(format: ImageFormat, metadata: [String: Any] = [:]) throws -> Data {
        guard let cgImage else { throw SaneError.cannotGenerateImage }
        
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
            throw SaneError.cannotGenerateImage
        }

        CGImageDestinationSetProperties(destination, options as CFDictionary)
        CGImageDestinationAddImage(destination, cgImage, options as CFDictionary)
        guard CGImageDestinationFinalize(destination) else {
            throw SaneError.cannotGenerateImage
        }
        return outputData as Data
    }
}
