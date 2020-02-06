//
//  DeviceAuthentication.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

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
    
    public func password(splitToMaxLength: Bool, md5DigestUsing md5string: String?) -> String? {
        guard let password = self.password else { return nil }
        let splitPassword = String(password.subarray(maxCount: Int(SANE_MAX_PASSWORD_LEN)))
        guard let md5string = md5string else { return splitPassword }
        return "$MD5$" + (md5string + splitPassword).md5()
    }
}
