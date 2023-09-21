//
//  SnapshotConfig.swift
//  Backlit
//
//  Created by syan on 19/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation
import UIKit

struct SnapshotConfig: Codable {
    // MARK: Kind
    enum Kind: Codable {
        case devicePreview, deviceOptions, deviceOptionPopup, other
    }
    var kind: Kind = .other
    
    // MARK: Host
    var hostAddress: String?
    var hostName: String?
    
    // MARK: Preview
    var previewImagePath: String?
    var previewImage: UIImage? {
        guard let previewImagePath else { return nil }
        return UIImage(contentsOfFile: previewImagePath)
    }
    var previewCrop: CGRect = .init(x: 0, y: 0, width: 1, height: 1)
    
    // MARK: Mock scan
    var mockScan: Bool = false
    var mockScanImagePath: String?
    var mockScanImage: UIImage? {
        guard let mockScanImagePath else { return nil }
        return UIImage(contentsOfFile: mockScanImagePath)
    }

    // MARK: Gallery
    var galleryImagesPaths: [[URL]] = []
}
