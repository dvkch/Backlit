//
//  Bundle+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

extension Bundle {
    
    var localizedName: String? {
        return object(forInfoDictionaryKey: kCFBundleNameKey as String) as? String
    }

    var commercialVersion: String {
        return (infoDictionary?["CFBundleShortVersionString"] as? String) ?? "0.0"
    }

    var buildVersion: String {
        return (infoDictionary?["CFBundleVersion"] as? String) ?? "0"
    }
    
    var fullVersion: String {
        return commercialVersion + "(" + buildVersion + ")"
    }
}

