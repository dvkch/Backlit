//
//  URL+SY.swift
//  Backlit
//
//  Created by syan on 22/09/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

extension URL {
    func temporaryCopy() throws -> URL {
        let tempURL = FileManager.default.temporaryDirectory.appendingPathComponent(lastPathComponent)
        try FileManager.default.copyItem(at: self, to: tempURL)
        return tempURL
    }
}
