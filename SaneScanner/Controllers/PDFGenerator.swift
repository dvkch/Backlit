//
//  PDFGenerator.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

enum PDFGeneratorError: Error {
    case noImages
    case cannotOpenImages
    case cannotOpenImage(url: URL)
    case invalidImageData(url: URL)
    case invalidJPEGQuality(value: CGFloat)
    case pdfCouldntBeGenerated
}

extension PDFGeneratorError: LocalizedError {
    var errorDescription: String? {
        switch self {
        case .noImages:                         return "ERROR MESSAGE PDF NO IMAGES".localized
        case .pdfCouldntBeGenerated:            return "ERROR MESSAGE PDF COULD NOT BE GENERATED".localized
        case .cannotOpenImages:                 return "ERROR MESSAGE CANNOT OPEN IMAGES".localized
        case .cannotOpenImage(let url):         return String(format: "ERROR MESSAGE CANNOT OPEN IMAGE %@".localized, url.deletingPathExtension().lastPathComponent)
        case .invalidImageData(let url):        return String(format: "ERROR MESSAGE INVALID IMAGE DATA %@".localized, url.deletingPathExtension().lastPathComponent)
        case .invalidJPEGQuality(let value):    return String(format: "ERROR MESSAGE INVALID JPEG QUALITY %.0lf".localized, value)
        }
    }
}

class PDFGenerator: NSObject {

    static func generatePDF(destination pdfURL: URL, images imagesURLs: [URL], aspectRatio: CGFloat, jpegQuality: CGFloat, fixedPageSize: Bool) throws {
        guard !imagesURLs.isEmpty else {
            throw PDFGeneratorError.noImages
        }
        
        guard jpegQuality > 0 && jpegQuality <= 1 else {
            throw PDFGeneratorError.invalidJPEGQuality(value: jpegQuality)
        }
        
        var greatestSize = CGSize.zero
        var minRatio = CGFloat(1)
        var maxRatio = CGFloat(1)
        
        for imageURL in imagesURLs {
            guard let size = UIImage.sizeOfImage(at: imageURL) else {
                throw PDFGeneratorError.cannotOpenImage(url: imageURL)
            }
            
            greatestSize.width  = max(greatestSize.width,  size.width )
            greatestSize.height = max(greatestSize.height, size.height)
            minRatio = min(minRatio, size.width / size.height)
            maxRatio = max(maxRatio, size.width / size.height)
        }
        
        guard greatestSize != .zero else {
            throw PDFGeneratorError.cannotOpenImages
        }
        
        var pdfSize = greatestSize
        if aspectRatio != 0 {
            // We create a size that has the wanted ratio, and the same area (w*h) than greatestSize. This
            // should prevent loosing too much data, or upscaling images too much.
            pdfSize.width  = sqrt(aspectRatio * greatestSize.width * greatestSize.height)
            pdfSize.height = pdfSize.width / aspectRatio
        }
        
        UIGraphicsBeginPDFContextToFile(pdfURL.path, CGRect(origin: .zero, size: pdfSize), nil)
        
        for imageURL in imagesURLs {
            guard var dataProvider = CGDataProvider(url: imageURL as CFURL) else {
                throw PDFGeneratorError.cannotOpenImage(url: imageURL)
            }
            
            var tempCGImage: CGImage?
            var sourceIsPNG = false
            
            tempCGImage = CGImage(jpegDataProviderSource: dataProvider, decode: nil, shouldInterpolate: true, intent: .defaultIntent)
            
            if tempCGImage == nil {
                tempCGImage = CGImage(pngDataProviderSource: dataProvider, decode: nil, shouldInterpolate: true, intent: .defaultIntent)
                sourceIsPNG = true
            }
            
            guard var cgImage = tempCGImage else {
                throw PDFGeneratorError.invalidImageData(url: imageURL)
            }
            
            // encode PNG data to JPEG if asked to
            if sourceIsPNG, let jpegData = UIImage(cgImage: cgImage).jpegData(compressionQuality: jpegQuality) {
                dataProvider = CGDataProvider(data: jpegData as CFData)!
                cgImage = CGImage(jpegDataProviderSource: dataProvider, decode: nil, shouldInterpolate: true, intent: .defaultIntent)!
            }
            
            // determines size that fits inside the PDF bounds, using as much space as possible, while
            // keeping the original image ratio
            let imageSize = UIImage.sizeOfImage(at: imageURL)!
            let sizeThatFits: CGSize
            if imageSize.width / imageSize.height > pdfSize.width / pdfSize.height {
                sizeThatFits = CGSize(width: pdfSize.width, height: imageSize.height * pdfSize.width / imageSize.width)
            }
            else {
                sizeThatFits = CGSize(width: imageSize.width * pdfSize.height / imageSize.height, height: pdfSize.height)
            }
            
            if fixedPageSize {
                // add image centered inside the page, page has the same size as the document
                UIGraphicsBeginPDFPage()
                
                var rect = CGRect.zero
                rect.origin.x = (pdfSize.width  - sizeThatFits.width ) / 2
                rect.origin.y = (pdfSize.height - sizeThatFits.height) / 2
                rect.size = sizeThatFits
                
                UIImage(cgImage: cgImage).draw(in: rect)
            }
            else
            {
                // add image in a page of the same size as the image
                let rect = CGRect(origin: .zero, size: sizeThatFits)
                UIGraphicsBeginPDFPageWithInfo(rect, nil)
                UIImage(cgImage: cgImage).draw(in: rect)
            }
        }
        
        UIGraphicsEndPDFContext()
        
        guard FileManager.default.fileExists(atPath: pdfURL.path) else {
            throw PDFGeneratorError.pdfCouldntBeGenerated
        }
    }
}
