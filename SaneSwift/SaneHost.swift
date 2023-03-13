//
//  SaneTypes.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 20/02/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

public class SaneHost: Codable, WithID {
    public var hostname: String
    public var displayName: String
    
    public var id: String {
        return hostname
    }
    
    public init(hostname: String, displayName: String) {
        self.hostname = hostname.trimmingCharacters(in: .whitespacesAndNewlines)
        self.displayName = displayName.trimmingCharacters(in: .whitespacesAndNewlines)
        if self.displayName.isEmpty {
            self.displayName = self.hostname
        }
    }
    
    private enum CodingKeys: String, CodingKey {
        case hostname = "host"
        case displayName = "name"
    }
}

extension SaneHost: Equatable {
    public static func ==(lhs: SaneHost, rhs: SaneHost) -> Bool {
        return lhs.id == rhs.id
    }
}
