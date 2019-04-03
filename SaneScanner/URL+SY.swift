//
//  URL+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 29/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation

extension URL {
    
    var creationDate: Date? {
        do {
            return try resourceValues(forKeys: Set([URLResourceKey.creationDateKey])).creationDate
        }
        catch {
            return nil
        }
    }
    
    var isDirectory: Bool? {
        do {
            return try resourceValues(forKeys: Set([URLResourceKey.isDirectoryKey])).isDirectory
        }
        catch {
            return nil
        }
    }
}
