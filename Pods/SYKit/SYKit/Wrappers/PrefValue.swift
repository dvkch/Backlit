//
//  PrefValue.swift
//  SYKit
//
//  Created by syan on 18/05/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

// https://betterprogramming.pub/five-property-wrappers-to-de-clutter-your-ios-code-f5ee6fa4e52e
@propertyWrapper
public class PrefValue<T: Codable>: NSObject {
    
    // MARK: Init
    public init(key: String, defaultValue: T, local: UserDefaults = .standard, ubiquitous: NSUbiquitousKeyValueStore? = .default, notification: Notification.Name? = nil) {
        self.key = key
        self.defaultValue = defaultValue
        self.local = local
        self.ubiquitous = ubiquitous
        self.notification = notification
        super.init()

        if let ubiquitous {
            // https://stackoverflow.com/a/13476127/1439489
            ubiquitous.set(Int.random(in: 0..<100), forKey: "random_key_to_start_syncing")
            ubiquitous.synchronize()

            NotificationCenter.default.addObserver(
                self, selector: #selector(self.ubiquitousStoreChanged(notification:)),
                name: NSUbiquitousKeyValueStore.didChangeExternallyNotification, object: ubiquitous
            )
        }
    }

    // MARK: Internal
    private var key: String
    private var defaultValue: T
    private var local: UserDefaults
    private var ubiquitous: NSUbiquitousKeyValueStore?
    private var notification: Notification.Name?
    
    // MARK: Properties
    public var wrappedValue: T {
        get {
            guard let data = local.data(forKey: key) else { return defaultValue }
            do {
                return try JSONDecoder().decode(T.self, from: data)
            }
            catch {
                print("Couldn't decode value for \(key): \(error)")
                return defaultValue
            }
        }
        set {
            do {
                let data = try JSONEncoder().encode(newValue)
                local.set(data, forKey: key)
                ubiquitous?.set(data, forKey: key)
                ubiquitous?.synchronize()
            }
            catch {
                print("Couldn't encode value for \(key): \(error)")
            }
            postNotification()
        }
    }
    
    // MARK: Sync
    private func postNotification() {
        if let notification {
            DispatchQueue.main.async {
                NotificationCenter.default.post(name: notification, object: nil)
            }
        }
    }

    @objc private func ubiquitousStoreChanged(notification: Notification) {
        guard let ubiquitous else { return }
        guard let reason = notification.userInfo?[NSUbiquitousKeyValueStoreChangeReasonKey] as? Int else { return }

        switch reason {
        case NSUbiquitousKeyValueStoreQuotaViolationChange:
            print("Over quota")
            
        case NSUbiquitousKeyValueStoreAccountChange:
            print("Account changed")
            
        case NSUbiquitousKeyValueStoreInitialSyncChange, NSUbiquitousKeyValueStoreServerChange:
            guard let keys = notification.userInfo?[NSUbiquitousKeyValueStoreChangedKeysKey] as? [String] else { return }
            guard keys.contains(key) else { return }

            let value = ubiquitous.object(forKey: key)
            if let data = value as? Data {
                // added / updated
                local.setValue(data, forKey: key)
                postNotification()
            }
            else if let value {
                // added / updated, but unknown type
                print("updated value for key \(key), but unknown value type \(type(of: value))")
            }
            else {
                // deleted
                local.removeObject(forKey: key)
                postNotification()
            }
            
        default:
            print("Unknown sync reason:", reason)
        }
    }
}
