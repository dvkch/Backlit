//
//  ScanNC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

@objc class ScanNC: UINavigationController {
    
    @objc var customToolbar: Toolbar? {
        return self.toolbar as? Toolbar
    }
    
    @objc override init(navigationBarClass: AnyClass?, toolbarClass: AnyClass?) {
        super.init(navigationBarClass: navigationBarClass, toolbarClass: Toolbar.self)
    }
    
    @objc init() {
        super.init(navigationBarClass: nil, toolbarClass: Toolbar.self)
    }
    
    override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: Bundle?) {
        super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
