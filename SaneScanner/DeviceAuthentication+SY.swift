//
//  DeviceAuthentication.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import SaneSwift
import KeychainAccess

extension DeviceAuthentication {
    private static var keychain: Keychain {
        return Keychain(service: Bundle.main.bundleIdentifier!)
    }
    
    private static func authenticationName(from deviceName: String) -> String {
        // authentication is asked with: net:192.168.69.42:pixma
        // but real device name usually is: net:192.168.69.42:pixma:04A9173B_610308
        return deviceName.split(separator: ":").subarray(maxCount: 3).joined(separator: ":")
    }

    private static func userKey(from deviceName: String) -> String {
        return authenticationName(from: deviceName) + "-user"
    }
    
    private static func passKey(from deviceName: String) -> String {
        return authenticationName(from: deviceName) + "-pass"
    }
    
    static func saved(for device: String) -> DeviceAuthentication? {
        if let user = keychain[userKey(from: device)],
           let pass = keychain[passKey(from: device)]
        {
            return DeviceAuthentication(username: user, password: pass)
        }
        return nil
    }

    static func forget(for device: String) {
        try? keychain.remove(userKey(from: device))
        try? keychain.remove(passKey(from: device))
    }
    
    func save(for device: String) {
        type(of: self).keychain[type(of: self).userKey(from: device)] = self.username(splitToMaxLength: false)
        type(of: self).keychain[type(of: self).passKey(from: device)] = self.password(splitToMaxLength: false, md5DigestUsing: nil)
    }
}
