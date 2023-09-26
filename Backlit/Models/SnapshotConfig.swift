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

extension SnapshotConfig {
    static func testConfig(bundle: Bundle) -> SnapshotConfig {
        var config = SnapshotConfig()
        config.kind = .other
        config.hostName = "Roost"
        config.hostAddress = "192.168.69.42"
        config.previewImagePath = bundle.path(forResource: "preview_image.jpg", ofType: nil)!
        config.previewCrop = CGRect(x: 0.45, y: 0.45, width: 0.55, height: 0.55)
        config.mockScan = true
        config.mockScanImagePath = config.previewImagePath
        config.galleryImagesPaths = [
            ["A1", "A2", "A3"],
            ["B1", "B2", "B3"],
            ["C1"],
            ["D1", "D2"],
            ["E1"],
            ["F1", "F2"],
            ["G1", "G2"],
            ["H1", "H2"],
            ["I1", "I2", "I3"]
        ].map { names in
            names.compactMap {
                bundle.url(forResource: "gallery-\($0).jpg", withExtension: nil)
            }
        }
        return config
    }
}
