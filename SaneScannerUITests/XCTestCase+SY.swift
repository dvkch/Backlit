//
//  XCTestCase+SY.swift
//  SaneScannerUITests
//
//  Created by Stanislas Chevallier on 26/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation
import XCTest

extension XCTestCase {
    func localizedString(key:String) -> String {
        let mainBundle = Bundle(for: SaneScannerUITests.self)
        
        var localizationPath = mainBundle.path(forResource: deviceLanguage, ofType: "lproj")
        if (localizationPath == nil) {
            let language = deviceLanguage.components(separatedBy: "-").first!
            localizationPath = mainBundle.path(forResource: language, ofType: "lproj")
        }
        
        let localizationBundle = Bundle(path: localizationPath!)
        return NSLocalizedString(key, bundle:localizationBundle!, comment: "")
    }
    
    enum WaitType {
        case exist, notExist
        case beEnabled, beDisabled
        case beHittable, beUnhittable

        var expectation: NSPredicate {
            switch self {
            case .exist:        return NSPredicate(format: "exists == %@", NSNumber(booleanLiteral: true))
            case .notExist:     return NSPredicate(format: "exists == %@", NSNumber(booleanLiteral: false))
            case .beEnabled:    return NSPredicate(format: "isEnabled == %@", NSNumber(booleanLiteral: true))
            case .beDisabled:   return NSPredicate(format: "isEnabled == %@", NSNumber(booleanLiteral: false))
            case .beHittable:   return NSPredicate(format: "isHittable == %@", NSNumber(booleanLiteral: true))
            case .beUnhittable: return NSPredicate(format: "isHittable == %@", NSNumber(booleanLiteral: false))
            }
        }

        var opposite: WaitType {
            switch self {
            case .exist: return .notExist
            case .notExist: return .exist
            case .beEnabled: return .beDisabled
            case .beDisabled: return .beEnabled
            case .beHittable: return .beUnhittable
            case .beUnhittable: return .beHittable
            }
        }
    }
    
    func wait(for element: XCUIElement, to waitType: WaitType, timeout: TimeInterval = 5, thenSwitch: Bool = false, switchTimeout: TimeInterval = 30) {
        expectation(for: waitType.expectation, evaluatedWith: element, handler: nil)
        waitForExpectations(timeout: timeout, handler: nil)
        
        if thenSwitch {
            wait(for: element, to: waitType.opposite, timeout: switchTimeout, thenSwitch: false)
        }
    }
    
    func waitForTableViewRefreshControl() {
        wait(for: XCUIApplication().staticTexts[localizedString(key: "LOADING")], to: .exist, timeout: 5, thenSwitch: true, switchTimeout: 30)
    }
    
    func waitForDeviceOpening() {
        wait(for: XCUIApplication().cells["loading_device"], to: .notExist, timeout: 30)
    }
}
