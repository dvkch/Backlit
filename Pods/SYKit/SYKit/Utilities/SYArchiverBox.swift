//
//  SYArchiverBox.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 02/07/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

public struct SYArchiverBox<T: NSObject & NSCoding>: Codable {
    public let value: T
    
    public init(_ value: T) {
        self.value = value
    }
    
    public init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        let data = try container.decode(Data.self)
        
        if #available(iOS 13.1, tvOS 13.1, *) {
            guard let objectValue = try NSKeyedUnarchiver.unarchivedObject(ofClass: T.self, from: data) else {
                throw DecodingError.dataCorruptedError(in: container, debugDescription: "Couldn't unarchive object")
            }
            self.value = objectValue
        }
        else {
            guard let objectValue = NSKeyedUnarchiver.unarchiveObject(with: data) else {
                throw DecodingError.dataCorruptedError(in: container, debugDescription: "Couldn't unarchive object")
            }
            guard let castedValue = objectValue as? T else {
                throw DecodingError.dataCorruptedError(in: container, debugDescription: "Couldn't cast object to type \(T.self)")
            }
            self.value = castedValue
        }
    }
    
    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        let data: Data
        if #available(iOS 13.1, tvOS 13.1, *) {
            data = try NSKeyedArchiver.archivedData(withRootObject: value, requiringSecureCoding: false)
        }
        else {
            data = NSKeyedArchiver.archivedData(withRootObject: value)
        }
        try container.encode(data)
    }
}
