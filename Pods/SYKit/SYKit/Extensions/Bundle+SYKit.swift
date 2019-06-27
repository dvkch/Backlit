//
//  Bundle+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public extension Bundle {
    @objc(sy_localizedName)
    var localizedName: String? {
        return object(forInfoDictionaryKey: kCFBundleNameKey as String) as? String
    }
    
    @objc(sy_commercialVersion)
    var commercialVersion: String {
        return (infoDictionary?["CFBundleShortVersionString"] as? String) ?? "0.0"
    }
    
    @objc(sy_buildVersion)
    var buildVersion: String {
        return (infoDictionary?["CFBundleVersion"] as? String) ?? "0"
    }
    
    @objc(sy_fullVersion)
    var fullVersion: String {
        return commercialVersion + " (" + buildVersion + ")"
    }
}
