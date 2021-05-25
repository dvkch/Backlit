//
//  SaneScannerUITests.swift
//  SaneScannerUITests
//
//  Created by Stanislas Chevallier on 06/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

import XCTest

class SaneScannerUITests: XCTestCase {
    override func setUp() {
        super.setUp()
        
        continueAfterFailure = false
        let app = XCUIApplication()
        Snapshot.setupSnapshot(app)
        app.launchArguments.append("DOING_SNAPSHOT")
        app.launchArguments.append(snapshotType)
        app.launchArguments.append("SNAPSHOT_TEST_IMAGE_PATH=" + snapshotTestImagePath!)
        app.launchArguments.append("SNAPSHOT_HOST=" + snapshotHost)
        app.launch()
        
        if UIDevice.current.userInterfaceIdiom == .pad {
            XCUIDevice.shared.orientation = .landscapeRight
        }
    }

    var snapshotType: String { "" }
    let snapshotHost: String = "192.168.69.42"
    let snapshotDevice: String = "Canon PIXMA MP270"
    let snapshotTestImagePath = Bundle(for: SaneScannerUITests.self).path(forResource: "test_scan_image", ofType: "png")
}

class SaneScannerUITests_Others : SaneScannerUITests {
    func testDevices() {
        waitForTableViewRefreshControl()
        Snapshot.snapshot("01-Devices")
    }
    
    func testSettings() {
        waitForTableViewRefreshControl()

        XCUIApplication().navigationBars["SaneScanner"].buttons["settings"].tap()
        Snapshot.snapshot("05-Settings")
    }
}

class SaneScannerUITests_Preview : SaneScannerUITests {
    override var snapshotType: String { "SNAPSHOT_PREVIEW" }
    
    func testDeviceWithPreview() {
        waitForTableViewRefreshControl()

        XCUIApplication().tables.staticTexts[snapshotDevice].tap()
        waitForDeviceOpening()
        
        Snapshot.snapshot("02-DeviceWithPreview")
    }
}

class SaneScannerUITests_Options : SaneScannerUITests {
    override var snapshotType: String { "SNAPSHOT_OPTIONS" }
    
    func testDeviceWithOptions() {
        waitForTableViewRefreshControl()
        
        XCUIApplication().tables.staticTexts[snapshotDevice].tap()
        waitForDeviceOpening()

        Snapshot.snapshot("03-DeviceWithOptions")
    }
}

class SaneScannerUITests_OptionPopup : SaneScannerUITests {
    override var snapshotType: String { "SNAPSHOT_OPTION_POPUP" }
    
    func testDeviceWithOptionsPopup() {
        waitForTableViewRefreshControl()
        
        XCUIApplication().tables.staticTexts[snapshotDevice].tap()
        waitForDeviceOpening()

        Snapshot.snapshot("04-DeviceWithOptionPopup")
    }
}
