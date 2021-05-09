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

    func presentHostInputAlert(title: String, message: String, add: String, cancel: String, completion: (String) -> ())

    func presentAuthInputAlert(title: String, message: String, usernamePlaceholder: String, passwordPlaceholder: String, continue: String, remember: String, cancel: String, completion: (_ username: String?, _ password: String?, _ remember: Bool) -> ())

    func dropdown(options: [CatalystDropdownValueProtocol], selectedIndex: Int, disabled: Bool, changed: @escaping (CatalystDropdownValueProtocol) -> ()) -> CatalystView
    
    func button(title: String, completion: @escaping () -> ()) -> CatalystView
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

@objc(CatalystDropdownValue) public protocol CatalystDropdownValueProtocol: NSObjectProtocol {
    var title: String { get }
    var value: NSObject? { get }
    
    init(title: String, value: NSObject?)
}

class CatalystDropdownValue: NSObject, NSCopying, CatalystDropdownValueProtocol {
    let title: String
    let value: NSObject?

    required init(title: String, value: NSObject?) {
        self.title = title
        self.value = value
    }
    
    func copy(with zone: NSZone? = nil) -> Any {
        return type(of: self).init(title: title, value: value)
    }

    public override var description: String {
        return title
    }
}
