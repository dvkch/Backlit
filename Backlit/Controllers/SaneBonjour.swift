//
//  SaneBonjour.swift
//  Backlit
//
//  Created by syan on 24/02/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation
import SaneSwift

protocol SaneBonjourDelegate: NSObjectProtocol {
    func saneBonjour(_ bonjour: SaneBonjour, updatedHosts: [SaneHost])
}

class SaneBonjour: NSObject {
    
    // MARK: Init
    static let shared = SaneBonjour()
    private override init() {
        super.init()
        browser.delegate = self
        
        NotificationCenter.default.addObserver(
            self, selector: #selector(start),
            name: UIApplication.didBecomeActiveNotification, object: nil
        )
    }
    
    // MARK: Properties
    weak var delegate: SaneBonjourDelegate?
    private(set) var isRunning: Bool = false
    
    private(set) var hosts: [SaneHost] = [] {
        didSet {
            delegate?.saneBonjour(self, updatedHosts: hosts)
        }
    }
    
    // MARK: Internal properties
    private let browser = NetServiceBrowser()
    private var services: [NetService] = []
    private var addresses: [NetService: [SaneHost]] = [:] {
        didSet {
            hosts = addresses
                .values.reduce([], +)
                .sorted(by: \.displayName)
        }
    }
    
    // MARK: Actions
    @objc func start() {
        guard !isRunning else { return }
        isRunning = true
        browser.searchForServices(ofType: "_sane-port._tcp", inDomain: "")
    }
    
    func stop() {
        browser.stop()
    }
}

extension SaneBonjour: NetServiceBrowserDelegate {
    func netServiceBrowserDidStopSearch(_ browser: NetServiceBrowser) {
        isRunning = false
    }

    func netServiceBrowser(_ browser: NetServiceBrowser, didNotSearch errorDict: [String : NSNumber]) {
        Logger.e(.bonjour, "Couldn't browse the network: \(errorDict)")
        isRunning = false
    }

    func netServiceBrowser(_ browser: NetServiceBrowser, didFind service: NetService, moreComing: Bool) {
        service.resolve(withTimeout: 5)
        service.delegate = self
        services.append(service)
    }

    func netServiceBrowser(_ browser: NetServiceBrowser, didRemove service: NetService, moreComing: Bool) {
        services.remove(service)
        addresses.removeValue(forKey: service)
    }
}

extension SaneBonjour: NetServiceDelegate {
    func netServiceDidResolveAddress(_ sender: NetService) {
        addresses[sender] = sender
            .netAddresses(resolvingHost: false)?
            .filter { $0.family == .ip4 }
            .map({ address in
                var displayName = address.host?.nilIfEmpty ?? sender.hostName
                displayName = displayName?.replacingOccurrences(of: ".local.", with: "")
                return SaneHost(hostname: address.ip, displayName: displayName ?? address.ip)
            })
    }

    func netService(_ sender: NetService, didNotResolve errorDict: [String : NSNumber]) {
        services.remove(sender)
        addresses.removeValue(forKey: sender)
    }
}
