//
//  ScanParameters.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation
import CoreGraphics

public struct ScanParameters: Equatable {
    public private(set) var currentlyAcquiredFrame: SANE_Frame
    public private(set) var acquiringLastFrame: Bool
    public private(set) var bytesPerLine: Int
    public private(set) var width: Int
    public private(set) var height: Int
    public private(set) var depth: Int
    public private(set) var cropArea: CGRect
    
    public init(
        currentlyAcquiredFrame: SANE_Frame, acquiringLastFrame: Bool,
        bytesPerLine: Int, width: Int, height: Int, depth: Int,
        cropArea: CGRect
    ) {
        self.currentlyAcquiredFrame = currentlyAcquiredFrame
        self.acquiringLastFrame = acquiringLastFrame
        self.bytesPerLine = bytesPerLine
        self.width = width
        self.height = height
        self.depth = depth
        self.cropArea = cropArea
    }
    
    init(cParams: SANE_Parameters, cropArea: CGRect) {
        self.init(
            currentlyAcquiredFrame: cParams.format,
            acquiringLastFrame:     cParams.last_frame == SANE_TRUE,
            bytesPerLine:           Int(cParams.bytes_per_line),
            width:                  Int(cParams.pixels_per_line),
            height:                 Int(cParams.lines),
            depth:                  Int(cParams.depth),
            cropArea:               cropArea
        )
    }
    
    var singleFrameEquivalent: ScanParameters {
        guard [SANE_FRAME_RED, SANE_FRAME_GREEN, SANE_FRAME_BLUE].contains(currentlyAcquiredFrame) else { return self }
        var adaptedParameters = self
        adaptedParameters.currentlyAcquiredFrame = SANE_FRAME_RGB
        adaptedParameters.acquiringLastFrame = true
        adaptedParameters.bytesPerLine *= 3
        return adaptedParameters
    }
}

extension ScanParameters {
    public var fileSize: Int {
        return bytesPerLine * height
    }
    
    public var numberOfChannels: Int {
        return currentlyAcquiredFrame == SANE_FRAME_RGB ? 3 : 1
    }
    
    public var expectedFramesCount: Int {
        return [SANE_FRAME_RED, SANE_FRAME_GREEN, SANE_FRAME_BLUE].contains(currentlyAcquiredFrame) ? 3 : 1
    }

    public var currentFrameIndex: Int {
        switch currentlyAcquiredFrame {
        case SANE_FRAME_GRAY:   return 0
        case SANE_FRAME_RGB:    return 0
        case SANE_FRAME_RED:    return 0
        case SANE_FRAME_GREEN:  return 1
        case SANE_FRAME_BLUE:   return 2
        default:
            fatalError("Unknown channel \(currentlyAcquiredFrame)")
        }
    }

    public var size: CGSize {
        return CGSize(width: width, height: height)
    }
}

extension ScanParameters : CustomStringConvertible {
    public var description: String {
        return "ScanParameters: \(width)x\(height)x\(depth), format: \(currentlyAcquiredFrame), isLast: \(acquiringLastFrame), bytesPerLine: \(bytesPerLine)"
    }
}
