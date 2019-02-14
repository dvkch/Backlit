//
//  DeviceAuthentication.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

@objc public class DeviceAuthentication: NSObject {
    var username: String?
    var password: String?
    
    @objc public init(username: String?, password: String?) {
        self.username = username
        self.password = password
    }
}
