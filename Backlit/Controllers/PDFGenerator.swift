//
//  PDFGenerator.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import AVKit

enum PDFGeneratorError: Error {
    case noImages
    case cannotOpenImages
    case cannotOpenImage(url: URL)
    case invalidImageData(url: URL)
    case pdfCouldntBeGenerated
}

extension PDFGeneratorError: LocalizedError {
    var errorDescription: String? {
        switch self {
        case .noImages:                         return "ERROR MESSAGE PDF NO IMAGES".localized
        case .pdfCouldntBeGenerated:            return "ERROR MESSAGE PDF COULD NOT BE GENERATED".localized
        case .cannotOpenImages:                 return "ERROR MESSAGE CANNOT OPEN IMAGES".localized
        case .cannotOpenImage(let url):
            return "ERROR MESSAGE CANNOT OPEN IMAGE %@".localized(url.deletingPathExtension().lastPathComponent)
        case .invalidImageData(let url):
            return "ERROR MESSAGE INVALID IMAGE DATA %@".localized(url.deletingPathExtension().lastPathComponent)
        }
    }
}

class PDFGenerator: NSObject {
    static func generatePDF(destination pdfURL: URL, images imagesURLs: [URL], pageSize: Preferences.PDFSize) throws {
        guard !imagesURLs.isEmpty else {
            throw PDFGeneratorError.noImages
        }
        
        var maxWidth = imagesURLs.compactMap { UIImage.sizeOfImage(at: $0)?.width }.max() ?? 0
        maxWidth = max(500, maxWidth)

        // autoreleasepool might not be required, but it allows using defer { ... } to make sure even an exception
        // doesn't prevent the context from being released
        try autoreleasepool {
            UIGraphicsBeginPDFContextToFile(pdfURL.path, .zero, nil)
            defer { UIGraphicsEndPDFContext() }
            
            for imageURL in imagesURLs {
                guard let image = UIImage(contentsOfFile: imageURL.path) else {
                    throw PDFGeneratorError.cannotOpenImage(url: imageURL)
                }
                
                switch pageSize {
                case .imageSize:
                    // add image in a page of the same size as the image
                    let scale = maxWidth / image.size.width
                    let pageBounds = CGRect(origin: .zero, size: image.size.applying(.init(scaleX: scale, y: scale)))
                    UIGraphicsBeginPDFPageWithInfo(pageBounds, nil)
                    image.draw(in: pageBounds)

                case .a4:
                    // add image centered inside the page, ratio is A4
                    let pageBounds = CGRect(origin: .zero, size: CGSize(area: image.size.area, aspectRatio: 210 / 297))
                    UIGraphicsBeginPDFPageWithInfo(pageBounds, nil)
                    
                    let imageRect = AVMakeRect(aspectRatio: image.size, insideRect: pageBounds)
                    image.draw(in: imageRect)

                case .usLetter:
                    // add image centered inside the page, ratio is US letter
                    let pageBounds = CGRect(origin: .zero, size: CGSize(area: image.size.area, aspectRatio: 8.5 / 11))
                    UIGraphicsBeginPDFPageWithInfo(pageBounds, nil)
                    
                    let imageRect = AVMakeRect(aspectRatio: image.size, insideRect: pageBounds)
                    image.draw(in: imageRect)
                }
            }
        }
        
        guard FileManager.default.fileExists(atPath: pdfURL.path) else {
            throw PDFGeneratorError.pdfCouldntBeGenerated
        }
    }
}
