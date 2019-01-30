//
//  UIImage+SY.m
//  SaneScanner
//
//  Created by Stan Chevallier on 04/03/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

import UIKit
import ImageIO

public extension UIImage {
    
    enum SaneSource {
        case data(Data)
        case file(URL)
        
        var provider: CGDataProvider? {
            switch self {
            // TODO: use releaseData callback variant?
            case .data(let data):   return CGDataProvider(data: data as CFData)
            case .file(let url):    return CGDataProvider(url: url as CFURL)
            }
        }
    }
    
    @objc static func sy_imageFromSane(data: Data?, url: URL?, parameters: SYSaneScanParameters) throws -> UIImage {
        if let data = data {
            return try self.sy_imageFromSane(source: UIImage.SaneSource.data(data), parameters: parameters)
        }
        if let url = url {
            return try self.sy_imageFromSane(source: UIImage.SaneSource.file(url), parameters: parameters)
        }
        throw NSError.ss_error(with: SSErrorCode_NoImageData)
    }
    
    static func sy_imageFromSane(source: SaneSource, parameters: SYSaneScanParameters) throws -> UIImage {
        guard let provider = source.provider else {
            throw NSError.ss_error(with: SSErrorCode_NoImageData)
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
            destNumberOfComponents = 1;
        default:
            throw NSError.ss_error(with: SSErrorCode_UnsupportedChannels)
        }
        
        // TODO: cleanup UInt -> Int
        guard let sourceImage = CGImage(
            width: Int(parameters.width),
            height: Int(parameters.height),
            bitsPerComponent: Int(parameters.depth),
            bitsPerPixel: Int(parameters.depth * parameters.numberOfChannels()),
            bytesPerRow: Int(parameters.bytesPerLine),
            space: colorSpace,
            bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.none.rawValue),
            provider: provider,
            decode: nil,
            shouldInterpolate: false,
            intent: CGColorRenderingIntent.defaultIntent
        ) else {
            throw NSError.ss_error(with: SSErrorCode_CannotGenerateImage)
        }

        // TODO: cleanup
        /*
         UIImage *img = [UIImage imageWithCGImage:sourceImageRef
         scale:1.
         orientation:UIImageOrientationUp];
         return img;
         */

        guard let context = CGContext(
            data: nil, // let iOS deal with allocating the memory
            width: Int(parameters.width),
            height: Int(parameters.height),
            bitsPerComponent: 8,
            bytesPerRow: Int(parameters.width) * destNumberOfComponents,
            space: colorSpace,
            bitmapInfo: destBitmapInfo.rawValue
        ) else {
            throw NSError.ss_error(with: SSErrorCode_CannotGenerateImage)
        }
        
        context.draw(sourceImage, in: CGRect(origin: .zero, size: parameters.size()))
        
        guard let destCGImage = context.makeImage() else {
            throw NSError.ss_error(with: SSErrorCode_CannotGenerateImage)
        }
        return UIImage(cgImage: destCGImage, scale: 1, orientation: .up)
    }
    
    
    @objc static func sy_imageFromIncompleteSane(data: Data, parameters: SYSaneScanParameters) throws -> UIImage {
        let incompleteParams = SYSaneScanParameters(forIncompleteDataOfLength: UInt(data.count), complete: parameters)!
        guard incompleteParams.fileSize() > 0 else {
            throw NSError.ss_error(with: SSErrorCode_NoImageData)
        }
        
        let image = try self.sy_imageFromSane(source: .data(data), parameters: incompleteParams)
        
        UIGraphicsBeginImageContextWithOptions(parameters.size(), true, 1.0)
        defer { UIGraphicsEndImageContext() }
        
        UIColor.white.setFill()
        UIBezierPath(rect: CGRect(origin: .zero, size: parameters.size())).fill()
        image.draw(in: CGRect(origin: .zero, size: image.size))
        
        guard let paddedImage = UIGraphicsGetImageFromCurrentImageContext() else {
            throw NSError.ss_error(with: SSErrorCode_CannotGenerateImage)
        }
        return paddedImage
    }
}

/*
// TODO: work on big images
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

