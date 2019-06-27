//
//  SYDeviceModel.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

@objc
public enum SYDeviceModel: Int, CaseIterable {
    // unknown
    case unknown
    
    // Simulator
    case simulator32bits
    case simulator64bits
    
    // iPod touch
    case iPodTouch1G
    case iPodTouch2G
    case iPodTouch3G
    case iPodTouch4G
    case iPodTouch5G
    case iPodTouch6G
    case iPodTouch7G
    
    // iPhone
    case iPhone
    case iPhone3G
    case iPhone3GS
    case iPhone4
    case iPhone4S
    case iPhone5
    case iPhone5C
    case iPhone5S
    case iPhoneSE
    case iPhone6
    case iPhone6Plus
    case iPhone6S
    case iPhone6SPlus
    case iPhone7
    case iPhone7Plus
    case iPhone8
    case iPhone8Plus
    case iPhoneX
    case iPhoneXs
    case iPhoneXsMax
    case iPhoneXr
    
    // iPad (9.7 inches)
    case iPad1
    case iPad2
    case iPad3
    case iPad4
    case iPadAir
    case iPadAir2
    case iPad5
    case iPad6
    case iPadAir3
    
    // iPad mini
    case iPadMini
    case iPadMini2
    case iPadMini3
    case iPadMini4
    case iPadMini5
    
    // iPad Pro
    case iPadPro1_12_9
    case iPadPro1_9_7
    case iPadPro2_12_9
    case iPadPro2_10_5
    case iPadPro3_12_9
    case iPadPro3_11
    
    // AppleTV
    case appleTV2Gen
    case appleTV3Gen
    case appleTV3GenRevA
    case appleTV4Gen
    case appleTV4K
    
    // AppleWatch
    case appleWatchGen1
    case appleWatchSeries1
    case appleWatchSeries2
    case appleWatchSeries3
    
    var hardwareStrings: [String] {
        switch self {
        case .unknown:          return []
            
        case .simulator32bits:  return ["i386"]
        case .simulator64bits:  return ["x86_64"]
            
        case .iPodTouch1G:      return ["iPod1,1"]
        case .iPodTouch2G:      return ["iPod2,1"]
        case .iPodTouch3G:      return ["iPod3,1"]
        case .iPodTouch4G:      return ["iPod4,1"]
        case .iPodTouch5G:      return ["iPod5,1"]
        case .iPodTouch6G:      return ["iPod7,1"]
        case .iPodTouch7G:      return ["iPod9,1"]
            
        case .iPhone:           return ["iPhone1,1"]
        case .iPhone3G:         return ["iPhone1,2"]
        case .iPhone3GS:        return ["iPhone2,1"]
        case .iPhone4:          return ["iPhone3,1", "iPhone3,2", "iPhone3,3"]
        case .iPhone4S:         return ["iPhone4,1"]
        case .iPhone5:          return ["iPhone5,1", "iPhone5,2"]
        case .iPhone5C:         return ["iPhone5,3", "iPhone5,4"]
        case .iPhone5S:         return ["iPhone6,1", "iPhone6,2"]
        case .iPhoneSE:         return ["iPhone8,4"]
            
        case .iPhone6:          return ["iPhone7,2"]
        case .iPhone6Plus:      return ["iPhone7,1"]
        case .iPhone6S:         return ["iPhone8,1"]
        case .iPhone6SPlus:     return ["iPhone8,2"]
        case .iPhone7:          return ["iPhone9,1", "iPhone9,3"]
        case .iPhone7Plus:      return ["iPhone9,2", "iPhone9,4"]
        case .iPhone8:          return ["iPhone10,1", "iPhone10,4"]
        case .iPhone8Plus:      return ["iPhone10,2", "iPhone10,5"]
        case .iPhoneX:          return ["iPhone10,3", "iPhone10,6"]
        case .iPhoneXs:         return ["iPhone11,2"]
        case .iPhoneXsMax:      return ["iPhone11,4", "iPhone11,6"]
        case .iPhoneXr:         return ["iPhone11,8"]
            
        case .iPad1:            return ["iPad1,1"]
        case .iPad2:            return ["iPad2,1", "iPad2,2", "iPad2,3", "iPad2,4"]
        case .iPad3:            return ["iPad3,1", "iPad3,2", "iPad3,3"]
        case .iPad4:            return ["iPad3,4", "iPad3,5", "iPad3,6"]
        case .iPadAir:          return ["iPad4,1", "iPad4,2", "iPad4,3"]
        case .iPadAir2:         return ["iPad5,3", "iPad5,4"]
        case .iPad5:            return ["iPad6,11", "iPad6,12"]
        case .iPad6:            return ["iPad7,5", "iPad7,6"]
        case .iPadAir3:         return ["iPad11,3", "iPad11,4"]
            
        case .iPadMini:         return ["iPad2,5", "iPad2,6", "iPad2,7"]
        case .iPadMini2:        return ["iPad4,4", "iPad4,5"]
        case .iPadMini3:        return ["iPad4,7", "iPad4,8", "iPad4,9"]
        case .iPadMini4:        return ["iPad5,1", "iPad5,2"]
        case .iPadMini5:        return ["iPad11,1", "iPad11,2"]
            
        case .iPadPro1_12_9:    return ["iPad6,7", "iPad6,8"]
        case .iPadPro1_9_7:     return ["iPad6,3", "iPad6,4"]
        case .iPadPro2_12_9:    return ["iPad7,1", "iPad7,2"]
        case .iPadPro2_10_5:    return ["iPad7,3", "iPad7,4"]
        case .iPadPro3_12_9:    return ["iPad8,5", "iPad8,6", "iPad8,7", "iPad8,8"]
        case .iPadPro3_11:      return ["iPad8,1", "iPad8,2", "iPad8,3", "iPad8,4"]
            
        case .appleTV2Gen:      return ["AppleTV2,1"]
        case .appleTV3Gen:      return ["AppleTV3,1"]
        case .appleTV3GenRevA:  return ["AppleTV3,2"]
        case .appleTV4Gen:      return ["AppleTV5,3"]
        case .appleTV4K:        return ["AppleTV6,2"]
            
        case .appleWatchGen1:   return ["Watch1,1", "Watch1,2"]
        case .appleWatchSeries1:return ["Watch2,6", "Watch2,7"]
        case .appleWatchSeries2:return ["Watch2,3", "Watch2,4"]
        case .appleWatchSeries3:return ["Watch3,1", "Watch3,2", "Watch3,3", "Watch3,4"]
        }
        
    }
    
    init?(hardwareString: String) {
        guard let model = SYDeviceModel.allCases.first(where: { $0.hardwareStrings.contains(hardwareString) }) else { return nil }
        self = model
    }
}


// ObjC bridge
@objcMembers
@available(swift, obsoleted: 1.0)
public class SYDevice: NSObject {
    
    // MARK: Init
    public init(model: SYDeviceModel) {
        self.model = model
    }
    
    public init?(hardwareString: String) {
        guard let model = SYDeviceModel(hardwareString: hardwareString) else { return nil }
        self.model = model
    }
    
    // MARK: Properties
    public let model: SYDeviceModel
    public var hardwareStrings: [String] {
        return model.hardwareStrings
    }
}

public extension UIDevice {
    @objc(sy_hardwareString)
    var hardwareString: String {
        var sysinfo = utsname()
        uname(&sysinfo)
        return String(bytes: Data(bytes: &sysinfo.machine, count: Int(_SYS_NAMELEN)), encoding: .ascii)!.trimmingCharacters(in: .controlCharacters)
    }
    
    @nonobjc
    var modelEnum: SYDeviceModel? {
        return SYDeviceModel(hardwareString: hardwareString)
    }
    
    @objc(sy_modelEnum)
    var objc_modelEnum: SYDeviceModel {
        return SYDeviceModel(hardwareString: hardwareString) ?? .unknown
    }
}

