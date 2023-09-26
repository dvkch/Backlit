//
//  Snapshot.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 26/04/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation
import SaneSwift

struct Snapshot {
    // MARK: Decoding
    static var isSnapshotting: Bool {
        ProcessInfo.processInfo.arguments.contains("DOING_SNAPSHOT")
    }

    private static var config: SnapshotConfig? = {
        guard let configString = argumentValue(for: "SNAPSHOT_CONFIG") else { return nil }
        let configData = configString.data(using: .utf8)!
        return try? JSONDecoder().decode(SnapshotConfig.self, from: configData)
    }()
    
    private static func argumentValue(for name: String) -> String? {
        let prefix = name + "="
        guard let argument = ProcessInfo.processInfo.arguments.first(where: { $0.hasPrefix(prefix) }) else { return nil }
        return argument.replacingOccurrences(of: prefix, with: "")
    }
    
    // MARK: Setup
    static func setup(_ closure: (SnapshotConfig) -> ()) {
        guard isSnapshotting, let config else { return }
        closure(config)
    }
}

extension SnapshotConfig {
    func setupHost() {
        guard let hostAddress, let hostName else { return }
        Sane.shared.configuration.hosts = [.init(hostname: hostAddress, displayName: hostName)]
    }
    
    func setupGallery() {
        GalleryManager.shared.galleryItems.forEach { GalleryManager.shared.deleteItem($0) }
        
        let secondsInDay: TimeInterval = 24 * 3600
        galleryImagesPaths.reversed().enumerated().forEach { (groupIndex, imagesURLs) in
            imagesURLs.reversed().enumerated().forEach { (imageIndex, imageURL) in
                let newURL = GalleryManager.shared.galleryFolder.appendingPathComponent(imageURL.lastPathComponent)
                let creationDate = Date(timeIntervalSinceNow: -TimeInterval(groupIndex) * secondsInDay - TimeInterval(imageIndex))
                try? FileManager.default.copyItem(at: imageURL, to: newURL)
                try? FileManager.default.setAttributes([.creationDate: creationDate], ofItemAtPath: newURL.path)
            }
        }
    }
}
