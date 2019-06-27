//
//  SaneConfig.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

public struct SaneConfig: Codable {
    
    // MARK: Codable
    private enum CodingKeys: String, CodingKey {
        case hosts = "hosts"
        case connectTimeout = "connect_timeout"
        case previewWithAutoColorMode = "preview_auto_color_mode"
        case showIncompleteScanImages = "show_incomplete_scan_images"
    }
    
    // MARK: Properties
    public var previewWithAutoColorMode: Bool = true {
        didSet {
            persistConfig()
        }
    }
    public var showIncompleteScanImages: Bool = true {
        didSet {
            persistConfig()
        }
    }
    public private(set) var hosts: [String] = [] {
        didSet {
            persistConfig()
        }
    }
    public var connectTimeout: Int = 30 {
        didSet {
            persistConfig()
        }
    }
    
    public mutating func addHost(_ host: String) {
        hosts.append(host)
    }
    
    public mutating func removeHost(_ host: String) {
        while let index = hosts.firstIndex(of: host) {
            hosts.remove(at: index)
        }
    }
    
    public mutating func clearHosts() {
        hosts = []
    }
}

// MARK: Persistence
internal extension SaneConfig {
    private func saneNetConfContent() -> String {
        var content = [String]()
        content.append(contentsOf: hosts)
        content.append("connect_timeout = " + String(connectTimeout))
        return content.joined(separator: "\n")
    }
    
    static func restored() -> SaneConfig? {
        guard let url = SaneConfig.saneSwiftConfigPlistURL, let data = try? Data(contentsOf: url) else {
            return nil
        }
        
        return try? PropertyListDecoder().decode(SaneConfig.self, from: data)
    }

    private func persistConfig() {
        guard let saneNetConfURL = SaneConfig.saneNetConfURL,  let saneSwiftConfigPlistURL = SaneConfig.saneSwiftConfigPlistURL else {
            print("Couldn't create config paths")
            return
        }
        
        guard let plistData = try? PropertyListEncoder().encode(self) else {
            print("Coudln't serialize SaneConfig to plist")
            return
        }
        
        try? plistData.write(to: saneSwiftConfigPlistURL)
        try? saneNetConfContent().write(to: saneNetConfURL, atomically: true, encoding: .utf8)
    }
}

// MARK: Paths
extension SaneConfig {
    static func makeConfigAvailableToSaneLib() {
        guard let url = configFolderURL else { return }
        FileManager.default.changeCurrentDirectoryPath(url.path)
    }
    
    private static var configFolderURL: URL? {
        guard let baseURL = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else { return nil }
        let saneConfigDirURL = baseURL.appendingPathComponent("SaneScanner", isDirectory: true)
        
        if !FileManager.default.fileExists(atPath: saneConfigDirURL.path) {
            try? FileManager.default.createDirectory(at: saneConfigDirURL, withIntermediateDirectories: true, attributes: nil)
        }
        
        return saneConfigDirURL
    }
    
    private static var saneNetConfURL: URL? {
        return configFolderURL?.appendingPathComponent("net.conf", isDirectory: false)
    }
    
    private static var saneSwiftConfigPlistURL: URL? {
        return configFolderURL?.appendingPathComponent("saneswift.plist", isDirectory: false)
    }
}
