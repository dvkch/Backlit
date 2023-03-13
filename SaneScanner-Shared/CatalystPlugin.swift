//
//  CatalystPlugin.swift
//  SaneScanner-CatalystPlugin
//
//  Created by Stanislas Chevallier on 03/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation

@objc(CatalystPlugin) public protocol CatalystPlugin: NSObjectProtocol {
    init()

    func presentHostInputAlert(title: String, message: String, hostPlaceholder: String, namePlaceholder: String, suggestedHost: String, suggestedName: String, add: String, cancel: String, completion: (String, String) -> ())

    func presentAuthInputAlert(title: String, message: String, usernamePlaceholder: String, passwordPlaceholder: String, continue: String, remember: String, cancel: String, completion: (_ username: String?, _ password: String?, _ remember: Bool) -> ())
}

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
