//
//  DeviceAuthentication.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

public struct DeviceAuthentication {
    private let username: String?
    private let password: String?
    
    public init(username: String?, password: String?) {
        self.username = username
        self.password = password
    }
    
    public func username(splitToMaxLength: Bool) -> String? {
        guard let username = self.username else { return nil }
        guard splitToMaxLength else { return username }
        return String(username.subarray(maxCount: Int(SANE_MAX_USERNAME_LEN)))
    }
    
    public func password(splitToMaxLength: Bool) -> String? {
        guard let password = self.password else { return nil }
        guard splitToMaxLength else { return password }
        return String(password.subarray(maxCount: Int(SANE_MAX_PASSWORD_LEN)))
    }
}
