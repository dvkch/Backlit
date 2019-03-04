//
//  DeviceOptionValueButton.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift

class Button : UIButton {
    var tapBlock: (() -> ())?
    
    override init(frame: CGRect) {
        super.init(frame: frame)
        addTarget(self, action: #selector(self.tap), for: .touchUpInside)
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        addTarget(self, action: #selector(self.tap), for: .touchUpInside)
    }
    
    @objc private func tap() {
        tapBlock?()
    }
}
