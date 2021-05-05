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
}
