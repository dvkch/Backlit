//
//  SaneScannerUITests.swift
//  SaneScannerUITests
//
//  Created by Stanislas Chevallier on 06/02/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

import XCTest

extension XCTestCase {
    
    func localizedString(key:String) -> String
    {
        let mainBundle = Bundle(for: SaneScannerUITests.self)
        
        var localizationPath = mainBundle.path(forResource: deviceLanguage, ofType: "lproj")
        
        if (localizationPath == nil)
        {
            let language = deviceLanguage.components(separatedBy: "-").first!;
            localizationPath = mainBundle.path(forResource: language, ofType: "lproj")
        }
        
        let localizationBundle = Bundle(path: localizationPath!)
        return NSLocalizedString(key, bundle:localizationBundle!, comment: "")
    }
    
    func wait(forElement element: XCUIElement, toExist exist: Bool , withTimeout timeout: TimeInterval)
    {
        expectation(for: NSPredicate(format: "exists == %@", NSNumber(booleanLiteral: exist)),
                    evaluatedWith: element,
                    handler: nil)
        
        waitForExpectations(timeout: timeout, handler: nil)
    }
    
    func waitForProgessHUDToAppearThenDisappear()
    {
        let app = XCUIApplication()
        let query = app.otherElements["SVProgressHUD"]
        
        wait(forElement: query, toExist: true,  withTimeout:  5)
        wait(forElement: query, toExist: false, withTimeout: 30)
    }
}

class SaneScannerUITests: XCTestCase {
    
    var snapshotType:String = ""
    
    override func setUp() {
        super.setUp()
        
        let testScanImagePath = Bundle(for: SaneScannerUITests.self).path(forResource: "test_scan_image", ofType: "png")
        
        continueAfterFailure = false
        let app = XCUIApplication()
        Snapshot.setupSnapshot(app)
        app.launchArguments.append("DOING_SNAPSHOT")
        app.launchArguments.append(snapshotType)
        app.launchArguments.append("SNAPSHOT_TEST_IMAGE_PATH=" + testScanImagePath!);
        app.launch()
        
        
        // Landscape on iPad
        if (UIDevice.current.userInterfaceIdiom == .pad)
        {
            XCUIDevice.shared().orientation = .landscapeRight
        }
    }
    
}

class SaneScannerUITests_Others : SaneScannerUITests {
    func testDevices()
    {
        waitForProgessHUDToAppearThenDisappear();
        Snapshot.snapshot("01-Devices", waitForLoadingIndicator: true)
    }
    
    func testSettings()
    {
        waitForProgessHUDToAppearThenDisappear();
        
        let app = XCUIApplication()
        app.navigationBars["SaneScanner"].buttons["settings"].tap()
        
        Snapshot.snapshot("05-Settings", waitForLoadingIndicator: true)
    }
}

class SaneScannerUITests_Preview : SaneScannerUITests {
    
    override func setUp() {
        snapshotType = "SNAPSHOT_PREVIEW"
        super.setUp()
    }
    
    func testDeviceWithPreview()
    {
        waitForProgessHUDToAppearThenDisappear();
        
        let app = XCUIApplication()
        
        let tablesQuery = app.tables
        tablesQuery.staticTexts["Canon PIXMA MP270"].tap()
        
        waitForProgessHUDToAppearThenDisappear()
        
        Snapshot.snapshot("02-DeviceWithPreview", waitForLoadingIndicator: true);
    }
}

class SaneScannerUITests_Options : SaneScannerUITests {
    
    override func setUp() {
        snapshotType = "SNAPSHOT_OPTIONS"
        super.setUp()
    }
    
    func testDeviceWithOptions()
    {
        waitForProgessHUDToAppearThenDisappear();
        
        let app = XCUIApplication()
        
        let tablesQuery = app.tables
        tablesQuery.staticTexts["Canon PIXMA MP270"].tap()
        
        waitForProgessHUDToAppearThenDisappear()
        
        Snapshot.snapshot("03-DeviceWithOptions", waitForLoadingIndicator: true);
    }
}

class SaneScannerUITests_OptionPopup : SaneScannerUITests {
    
    override func setUp() {
        snapshotType = "SNAPSHOT_OPTION_POPUP"
        super.setUp()
    }
    
    func testDeviceWithOptions()
    {
        waitForProgessHUDToAppearThenDisappear();
        
        let app = XCUIApplication()
        
        let tablesQuery = app.tables
        tablesQuery.staticTexts["Canon PIXMA MP270"].tap()
        
        waitForProgessHUDToAppearThenDisappear()
        
        Snapshot.snapshot("04-DeviceWithOptionPopup", waitForLoadingIndicator: true);
    }
}
