//
//  CatalystPlugin+iOS.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 05/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

@available(macCatalyst 10.0, *)
@available(iOS, unavailable)
func obtainCatalystPlugin() -> CatalystPlugin {
    let bundleFileName = "SaneScanner-CatalystPlugin.bundle"
    guard let bundleURL = Bundle.main.builtInPlugInsURL?.appendingPathComponent(bundleFileName), let bundle = Bundle(url: bundleURL) else {
        fatalError("Couldn't find CatalystPlugin")
    }
    bundle.load()
    
    let className = "SaneScanner_CatalystPlugin.CatalystPluginImplementation"
    guard let pluginClass = bundle.classNamed(className) as? CatalystPlugin.Type else {
        fatalError("Couldn't find CatalystPlugin")
    }

    return pluginClass.init()
}
