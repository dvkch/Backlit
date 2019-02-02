//
//  Sane.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

@objc public class SaneConfig: NSObject, Codable {
    
    // MARK: Codable
    private enum CodingKeys: String, CodingKey {
        case hosts = "hosts"
        case connectTimeout = "connect_timeout"
    }
    
    // MARK: Properties
    @objc public var previewWithAutoColorMode: Bool = true {
        didSet {
            persistConfig()
        }
    }
    @objc public var showIncompleteScanImages: Bool = true {
        didSet {
            persistConfig()
        }
    }
    @objc public private(set) var hosts: [String] = [] {
        didSet {
            persistConfig()
        }
    }
    @objc public var connectTimeout: Int = 30 {
        didSet {
            persistConfig()
        }
    }
    
    @objc func addHost(_ host: String) {
        hosts.append(host)
    }
    
    @objc func removeHost(_ host: String) {
        while let index = hosts.index(of: host) {
            hosts.remove(at: index)
        }
    }
    
    @objc func clearHosts() {
        hosts = []
    }
}

// MARK: Persistence
@objc extension SaneConfig {
    private func saneNetConfContent() -> String {
        var content = [String]()
        content.append(contentsOf: hosts)
        content.append("connect_timeout = " + String(connectTimeout))
        return content.joined(separator: "\n")
    }
    
    @objc class func restored() -> SaneConfig? {
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
