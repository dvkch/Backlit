//
//  SaneScannerUITests.swift
//  SaneScannerUITests
//
//  Created by Stanislas Chevallier on 06/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

import XCTest

class SaneScannerUITests: XCTestCase {
    let device = "LiDE 220"
    var config: SnapshotConfig = {
        var config = SnapshotConfig()
        config.kind = .other
        config.hostName = "Roost"
        config.hostAddress = "192.168.69.42"
        config.previewImagePath = Bundle(for: SaneScannerUITests.self).path(forResource: "preview_image.jpg", ofType: nil)!
        config.previewCrop = CGRect(x: 0.45, y: 0.45, width: 0.55, height: 0.55)
        config.mockScan = true
        config.mockScanImagePath = config.previewImagePath
        config.galleryImagesPaths = [
            ["A1", "A2", "A3"],
            ["B1", "B2", "B3"],
            ["C1"],
            ["D1", "D2"],
            ["E1"],
            ["F1", "F2"],
            ["G1", "G2"],
            ["H1", "H2"],
            ["I1", "I2", "I3"]
        ].map { names in
            names.compactMap {
                Bundle(for: SaneScannerUITests.self).url(forResource: "gallery-\($0).jpg", withExtension: nil)
            }
        }
        return config
    }()
    
    var app = XCUIApplication()
    
    override func setUp() {
        super.setUp()
        
        #if !targetEnvironment(macCatalyst)
        if UIDevice.current.userInterfaceIdiom == .pad {
            XCUIDevice.shared.orientation = .landscapeRight
        }
        #endif

        continueAfterFailure = false
        Snapshot.setupSnapshot(app)
        
        let configJSON = try! JSONEncoder().encode(config)
        app.launchArguments.append("DOING_SNAPSHOT")
        app.launchArguments.append("SNAPSHOT_CONFIG=" + String(data: configJSON, encoding: .utf8)!)
        app.launch()
    }
    
    func wait(for seconds: TimeInterval) {
        usleep(UInt32(seconds * TimeInterval(1_000_000)))
    }
}

class SaneScannerUITests_Others : SaneScannerUITests {
    func testDevices() {
        waitForTableViewRefreshControl()
        Snapshot.snapshot("01-Devices")
    }
    
    func testGallery() {
        #if !targetEnvironment(macCatalyst)
        waitForTableViewRefreshControl()

        app.cells["gallery-item-0"].tap()
        app.navigationBars.buttons[localizedString(key: "GALLERY OVERVIEW TITLE")].tap()

        Snapshot.snapshot("05-Gallery")
        #endif
    }
    
    func testSettings() {
        waitForTableViewRefreshControl()

        #if targetEnvironment(macCatalyst)
        app.windows.firstMatch.typeKey(",", modifierFlags: .command)
        #else
        app.navigationBars["SaneScanner"].buttons[localizedString(key: "PREFERENCES TITLE")].tap()
        #endif

        Snapshot.snapshot("06-Settings")
    }
}

class SaneScannerUITests_Preview : SaneScannerUITests {
    override func setUp() {
        config.kind = .devicePreview
        super.setUp()
    }
    
    func testDeviceWithPreview() {
        waitForTableViewRefreshControl()

        app.tables.staticTexts[device].tap()
        waitForDeviceOpening()
        
        Snapshot.snapshot("02-DeviceWithPreview")
    }
}

class SaneScannerUITests_Options : SaneScannerUITests {
    override func setUp() {
        config.kind = .deviceOptions
        super.setUp()
    }
    
    func testDeviceWithOptions() {
        waitForTableViewRefreshControl()
        
        app.tables.staticTexts[device].tap()
        waitForDeviceOpening()

        Snapshot.snapshot("03-DeviceWithOptions")
    }
}

class SaneScannerUITests_OptionPopup : SaneScannerUITests {
    override func setUp() {
        config.kind = .deviceOptionPopup
        super.setUp()
    }
    
    func testDeviceWithOptionsPopup() {
        waitForTableViewRefreshControl()
        
        app.tables.staticTexts[device].tap()
        waitForDeviceOpening()

        Snapshot.snapshot("04-DeviceWithOptionPopup")
    }
}
