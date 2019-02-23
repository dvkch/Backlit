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
        return username.substring(to: username.index(username.startIndex, offsetBy: max(username.count, Int(SANE_MAX_USERNAME_LEN))))
    }
    
    public func password(splitToMaxLength: Bool) -> String? {
        guard let password = self.password else { return nil }
        guard splitToMaxLength else { return password }
        return password.substring(to: password.index(password.startIndex, offsetBy: max(password.count, Int(SANE_MAX_PASSWORD_LEN))))
    }
}
