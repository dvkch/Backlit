//
//  GalleryImagePreviewVC.swift
//  Backlit
//
//  Created by syan on 07/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

#if !targetEnvironment(macCatalyst)
class GalleryImagePreviewVC: UIViewController {
    
    // MARK: Init
    init(item: GalleryItem) {
        self.item = item
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: UIViewController
    override func viewDidLoad() {
        super.viewDidLoad()
        
        imageView.item = item
        view.addSubview(imageView)
        imageView.snp.makeConstraints { make in
            make.width.equalTo(imageView.snp.height).multipliedBy(imageView.displayedImageSize.width / imageView.displayedImageSize.height)
            make.center.equalToSuperview()
            make.top.left.greaterThanOrEqualToSuperview()
            make.top.left.equalToSuperview().priority(.high)
        }
        
        preferredContentSize = imageView.displayedImageSize
    }
    
    // MARK: Properties
    let item: GalleryItem
    
    // MARK: View
    private let imageView = GalleryImageView()
    
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        preferredContentSize = imageView.bounds.size
    }
}
#endif
