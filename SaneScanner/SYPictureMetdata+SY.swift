//
//  SYPictureMetdata+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 05/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import SYPictureMetadata
import SaneSwift

extension SYMetadata {
    
    convenience init(device: Device, scanParameters: ScanParameters) {
        var resX: Int? = nil
        var resY: Int? = nil
        var res:  Int? = nil
        
        if let optionResX = device.standardOption(for: .resolutionX) as? DeviceOptionInt { resX = optionResX.value }
        if let optionResY = device.standardOption(for: .resolutionY) as? DeviceOptionInt { resY = optionResY.value }
        if let optionRes  = device.standardOption(for: .resolution)  as? DeviceOptionInt { res  = optionRes.value  }
        
        if let optionResX = device.standardOption(for: .resolutionX) as? DeviceOptionFixed { resX = Int(optionResX.value) }
        if let optionResY = device.standardOption(for: .resolutionY) as? DeviceOptionFixed { resY = Int(optionResY.value) }
        if let optionRes  = device.standardOption(for: .resolution)  as? DeviceOptionFixed { res  = Int(optionRes.value)  }
        
        let resXInches = resX ?? res
        let resYInches = resY ?? res
        
        var resXMeters: Int? = nil
        var resYMeters: Int? = nil
        
        if let resXInches = resXInches {
            resXMeters = Int(round(Double(resXInches) / 2.54 * 100))
        }
        
        if let resYInches = resYInches {
            resYMeters = Int(round(Double(resYInches) / 2.54 * 100))
        }
        
        self.init()
        
        metadataTIFF = SYMetadataTIFF()
        metadataTIFF.orientation = SYPictureTiffOrientation_TopLeft.rawValue as NSNumber
        metadataTIFF.make = device.vendor
        metadataTIFF.model = device.model
        metadataTIFF.software = (Bundle.main.localizedName ?? "") + " " + Bundle.main.fullVersion
        metadataTIFF.xResolution = resXInches.map(NSNumber.init)
        metadataTIFF.yResolution = resYInches.map(NSNumber.init)
        metadataTIFF.resolutionUnit = 2 // 2 = inches, let's hope it'll make sense for every device
        
        metadataPNG = SYMetadataPNG()
        metadataPNG.xPixelsPerMeter = resXMeters.map(NSNumber.init)
        metadataPNG.yPixelsPerMeter = resYMeters.map(NSNumber.init)
        
        metadataJFIF = SYMetadataJFIF()
        metadataJFIF.xDensity = resXInches.map(NSNumber.init)
        metadataJFIF.yDensity = resYInches.map(NSNumber.init)
    }
}
