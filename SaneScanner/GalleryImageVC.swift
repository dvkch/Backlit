//
//  GalleryImageVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class GalleryImageVC: UIViewController {

    // MARK: Init
    init(item: GalleryItem, index: Int) {
        self.item = item
        self.galleryIndex = index
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: UIViewController
    override func viewDidLoad() {
        super.viewDidLoad()
        
        imageView.contentMode = .scaleAspectFit
        view.addSubview(imageView)
        imageView.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        
        view.setNeedsLayout()
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        // lazy loading only when view will appear
        if imageView.image == nil {
            imageView.image = UIImage(contentsOfFile: item.URL.path)
        }
    }
    
    // MARK: Properties
    let item: GalleryItem
    let galleryIndex: Int
    
    // MARK: Views
    private let imageView = UIImageView()
}
