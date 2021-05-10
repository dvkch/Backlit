//
//  ScanParameters.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation
import CoreGraphics

public struct ScanParameters: Equatable {
    public let currentlyAcquiredChannel: SANE_Frame
    public let acquiringLastChannel: Bool
    public let bytesPerLine: Int
    public let width: Int
    public private(set) var height: Int
    public let depth: Int
    public let cropArea: CGRect
    
    public init(cParams: SANE_Parameters, cropArea: CGRect) {
        currentlyAcquiredChannel    = cParams.format
        acquiringLastChannel        = cParams.last_frame == SANE_TRUE
        bytesPerLine                = Int(cParams.bytes_per_line)
        depth                       = Int(cParams.depth)
        width                       = Int(cParams.pixels_per_line)
        height                      = Int(cParams.lines)
        self.cropArea               = cropArea
    }
    
    public func incompleteParameters(dataLength: Int) -> ScanParameters {
        var newParams = self
        newParams.height = dataLength / bytesPerLine
        return newParams
    }
}

extension ScanParameters {
    public var fileSize: Int {
        return bytesPerLine * height
    }
    
    public var numberOfChannels: Int {
        return currentlyAcquiredChannel == SANE_FRAME_RGB ? 3 : 1
    }
    
    public var size: CGSize {
        return CGSize(width: width, height: height)
    }
}

extension ScanParameters : CustomStringConvertible {
    public var description: String {
        return "ScanParameters: \(width)x\(height)x\(depth), channel: \(currentlyAcquiredChannel), isLast: \(acquiringLastChannel), bytesPerLine: \(bytesPerLine)"
    }
}
