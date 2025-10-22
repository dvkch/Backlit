//
//  BacklitUITests.swift
//  BacklitUITests
//
//  Created by syan on 06/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

import XCTest

class BacklitUITests: XCTestCase {
    let device = "LiDE 220"
    var config: SnapshotConfig = .testConfig(bundle: Bundle(for: BacklitUITests.self))
    
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

class BacklitUITests_Others : BacklitUITests {
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
        app.navigationBars["Backlit"].buttons[localizedString(key: "PREFERENCES TITLE")].tap()
        #endif

        Snapshot.snapshot("06-Settings")
    }
}

class BacklitUITests_Preview : BacklitUITests {
    override func setUp() {
        config.kind = .devicePreview
        super.setUp()
    }
    
    func testDeviceWithPreview() {
        guard UIDevice.current.userInterfaceIdiom == .phone else { return }

        waitForTableViewRefreshControl()

        app.tables.staticTexts[device].tap()
        waitForDeviceOpening()
        
        Snapshot.snapshot("02-DeviceWithPreview")
    }
}

class BacklitUITests_Options : BacklitUITests {
    override func setUp() {
        config.kind = .deviceOptions
        super.setUp()
    }
    
    func testDeviceWithOptions() {
        guard UIDevice.current.userInterfaceIdiom == .phone else { return }

        waitForTableViewRefreshControl()
        
        app.tables.staticTexts[device].tap()
        waitForDeviceOpening()

        Snapshot.snapshot("03-DeviceWithOptions")
    }
}

class BacklitUITests_OptionPopup : BacklitUITests {
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

class BacklitUITests_Video : BacklitUITests {
    func testDeviceWithVideo() {
        let recordingVideos = app.launchArguments.contains("--VIDEO_SNAPSHOTS")
        guard recordingVideos else { return }
        
        waitForTableViewRefreshControl()
        
        showcaseScan()
        #if !targetEnvironment(macCatalyst)
        showcaseGallery()
        #endif
        showcaseSettings()

        wait(for: 4)
    }
    
    private func showcaseScan() {
        // open device
        app.tables.staticTexts[device].go()
        waitForDeviceOpening()

        // preview
        let previewButton = app.buttons[localizedString(key: "DEVICE BUTTON UPDATE PREVIEW")]
        previewButton.go(duration: 0.1)
        wait(for: previewButton, to: .notExist, timeout: 5, thenSwitch: true, switchTimeout: 60)

        // select preview area
        let cropView = app.otherElements["crop-view"]
        let cropTopLeftDestination = cropView.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.47))
        let cropTop = app.otherElements["crop-side-top"]
        let cropLeft = app.otherElements["crop-side-left"]
        
        cropTop.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
            .press(forDuration: 0.5, thenDragTo: cropTopLeftDestination)
        wait(for: 0.5)

        cropLeft.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5))
            .press(forDuration: 0.5, thenDragTo: cropTopLeftDestination)
        wait(for: 1)

        // select color mode
        if UIDevice.current.userInterfaceIdiom == .phone {
            let navBarBottom = app.navigationBars[device]
                .coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 1))
                .screenPoint
            
            let scanModeHeaderTop = app.cells["mode"]
                .coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0))
                .withOffset(CGVector(dx: 0, dy: -32))
                .screenPoint
            
            let previewButtonStartPosition = previewButton
                .coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 1.1))
            let previewButtonEndPosition = previewButtonStartPosition
                .withOffset(CGVector(dx: 0, dy: navBarBottom.y - scanModeHeaderTop.y))

            previewButtonStartPosition
                .press(forDuration: 0.1, thenDragTo: previewButtonEndPosition)
        }
        app.cells["mode"].buttons.firstMatch.go()
        wait(for: 0.5)
        selectMenuItem(identifier: "mode-0", at: 0)
        wait(for: 1) // wait for options to refresh

        // select resolution
        app.cells["resolution"].buttons.firstMatch.go()
        wait(for: 0.5)
        selectMenuItem(identifier: "resolution-5", at: 5)
        wait(for: 1) // wait for options to refresh

        // scan
        if UIDevice.current.userInterfaceIdiom == .phone {
            app.tables.firstMatch.swipeDown()
        }
        let scanButton = app.buttons[localizedString(key: "ACTION SCAN")]
        scanButton.go()
        wait(for: scanButton, to: .notExist, timeout: 5, thenSwitch: true, switchTimeout: 60)
        wait(for: 1)
    }
    
    private func showcaseGallery() {
        // preview last scan
        app.cells["gallery-item-0"].firstMatch.go(duration: 1)
        wait(for: 1)
        
        // open last scan
        app.buttons[localizedString(key: "ACTION OPEN")].go()
        wait(for: 1)
        
        // back to gallery
        app.navigationBars.buttons[localizedString(key: "GALLERY OVERVIEW TITLE")].go()
        wait(for: 1)

        // make a pdf
        app.navigationBars.buttons[localizedString(key: "ACTION EDIT")].go()
        app.cells["gallery-grid-6-0"].tap()
        app.cells["gallery-grid-6-1"].tap()
        app.buttons["PDF"].go()
        wait(for: 1)
        app.buttons[localizedString(key: "SHARE AS PDF")].go()
        
        // (almost) print pdf
        let print = deviceLanguage.hasPrefix("fr") ? "Imprimer" : "Print"
        if #available(iOS 16, *) {
            app.otherElements["ActivityListView"].cells[print].go() // found using `po app` in debugger
        }
        else {
            app.otherElements["ActivityListView"].buttons[print].go() // found using `po app` in debugger
        }
        wait(for: 2)
        app.navigationBars.buttons[localizedString(key: "ACTION CANCEL")].go()

        // close gallery
        let done = deviceLanguage.hasPrefix("fr") ? "OK" : "Done"
        app.navigationBars.buttons[done].tap() // stop editing
        wait(for: 1)
        app.navigationBars.buttons[localizedString(key: "ACTION CLOSE")].go() // close
    }
    
    private func showcaseSettings() {
        #if targetEnvironment(macCatalyst)
        app.windows.firstMatch.typeKey(",", modifierFlags: .command)
        #else
        app.navigationBars.buttons[localizedString(key: "PREFERENCES TITLE")].go()
        #endif
        wait(for: 1)

        app.cells["ImageFormat"].buttons.firstMatch.go()
        selectMenuItem(identifier: "PNG", at: 1)
        wait(for: 1)

        app.navigationBars.buttons[localizedString(key: "ACTION CLOSE")].go() // close
        wait(for: 1)
        
        app.navigationBars.buttons["Backlit"].go()
    }
    
    private func selectMenuItem(identifier: String, at index: Int) {
        #if targetEnvironment(macCatalyst)
        for _ in 0...index {
            app.typeKey(XCUIKeyboardKey.downArrow.rawValue, modifierFlags: [])
        }
        app.typeKey(XCUIKeyboardKey.enter.rawValue, modifierFlags: [])
        #else
        app.buttons[identifier].go(duration: 0.5) // "color"
        #endif
    }
}
