//
//  SaneConfig.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

public struct SaneConfig: Codable, Equatable {
    
    // MARK: Properties
    public var previewWithAutoColorMode: Bool = true
    public var hosts: [SaneHost] = []
    public var transientdHosts: [SaneHost] = []
    public var connectTimeout: Int = 10
    
    // MARK: Codable
    private enum CodingKeys: String, CodingKey {
        case hosts = "hosts"
        case connectTimeout = "connect_timeout"
        case previewWithAutoColorMode = "preview_auto_color_mode"
    }
}

// MARK: Persistence
internal extension SaneConfig {
    static func restored() -> SaneConfig? {
        do {
            let data = try Data(contentsOf: SaneConfig.saneSwiftConfigPlistURL)
            return try PropertyListDecoder().decode(SaneConfig.self, from: data)
        }
        catch {
            print("Couldn't restore config: \(error)")
            return nil
        }
    }

    static func persist(_ config: SaneConfig) {
        // Step 1: save the configuration
        do {
            let plistData = try PropertyListEncoder().encode(config)
            try plistData.write(to: saneSwiftConfigPlistURL)
        }
        catch {
            SaneLogger.e(.sane, "Couldn't save config: \(error)")
        }
        
        // Step 2: expose the new config to SANE
        let hostsEnv = (config.hosts + config.transientdHosts).map(\.hostname).saneJoined()
        setenv("SANE_NET_HOSTS", hostsEnv, 1)
        setenv("SANE_NET_TIMEOUT", String(config.connectTimeout), 1)
    }
}

// MARK: Paths
internal extension SaneConfig {
    static func makeConfigAvailableToSaneLib() {
        setenv("SANE_CONFIG_DIR", configFolderURL.absoluteURL.path, 1)
        
        let dllConf = "net\n"
        try? dllConf.write(to: saneDllConfURL, atomically: true, encoding: .utf8)
    }
    
    private static var configFolderURL: URL {
        guard let baseURL = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else {
            fatalError("Should have access to config directory")
        }
        let saneConfigDirURL = baseURL.appendingPathComponent("SaneScanner", isDirectory: true)
        
        if !FileManager.default.fileExists(atPath: saneConfigDirURL.path) {
            try? FileManager.default.createDirectory(at: saneConfigDirURL, withIntermediateDirectories: true, attributes: nil)
        }
        
        return saneConfigDirURL
    }
    
    private static var saneDllConfURL: URL {
        return configFolderURL.appendingPathComponent("dll.conf", isDirectory: false)
    }
    
    private static var saneSwiftConfigPlistURL: URL {
        return configFolderURL.appendingPathComponent("saneswift.plist", isDirectory: false)
    }
}
