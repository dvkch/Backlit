//
//  URL+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public extension URL {
    var hostWithoutSubdomain: String? {
        let components = self.host?.components(separatedBy: ".") ?? []
        guard components.count > 2 else { return host }
        return components[(components.endIndex - 2)..<components.endIndex].joined(separator: ".")
    }
    
    func replacingPath(with path: String) -> URL? {
        var components = URLComponents(url: self, resolvingAgainstBaseURL: true)
        components?.path = path
        return components?.url
    }
    
    func replacingPathComponent(_ component: String, with string: String) -> URL {
        var components = URLComponents(url: self, resolvingAgainstBaseURL: true)!
        components.path = self.pathComponents.map { c in
            return c == component ? string : c
            }.joined(separator: "/")
        return components.url!
    }
}

public extension URL {
    var creationDate: Date? {
        return try? resourceValues(forKeys: Set([URLResourceKey.creationDateKey])).creationDate
    }
    
    var isDirectory: Bool? {
        return try? resourceValues(forKeys: Set([URLResourceKey.isDirectoryKey])).isDirectory
    }
}

public extension String {
    func asURL(relativeTo baseURL: URL? = nil) -> URL? {
        return URL(string: self, relativeTo: baseURL)?.absoluteURL
    }
}
