//
//  EmptyGalleryVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit
import Masonry

@objc class EmptyGalleryVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .white
        
        let text = NSMutableAttributedString()
        text.sy_appendString("GALLERY EMPTY TITLE".localized, font: .systemFont(ofSize: 17), color: .darkGray)
        text.sy_appendString("\n\n", font: .systemFont(ofSize: 15), color: .gray)
        text.sy_appendString("GALLERY EMPTY SUBTITLE".localized, font: .systemFont(ofSize: 15), color: .gray)

        label.attributedText = text
        label.numberOfLines = 0
        label.lineBreakMode = .byWordWrapping
        label.textAlignment = .center
        view.addSubview(label)
        
        label.mas_makeConstraints { (make) in
            make?.center.equalTo()(0)
            make?.width.lessThanOrEqualTo()(300)
        }
    }

    // MARK: Properties
    private let label = UILabel()
    
}
