//
//  GalleryNC.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit

#if !targetEnvironment(macCatalyst)
class GalleryNC: UINavigationController {

    // MARK: Init
    init(openedOn item: GalleryItem?) {
        super.init(rootViewController: gridVC)
        
        if let item, let index = GalleryManager.shared.galleryItems.firstIndex(of: item) {
            let imagesVC = GalleryImagesVC()
            imagesVC.initialIndex = index
            self.viewControllers = [gridVC, imagesVC]
        }
        else {
            self.viewControllers = [gridVC]
        }
        
        // make sure the back button has the right name when opening at specific index
        gridVC.loadViewIfNeeded()
        
        // the bar buttons are not super readable on a dark interface when a white image goes behind a toolbar
        // and using a lighter color isn't better. let's just make the blur effect less transparent (default
        // is systemChromeMaterial)
        navigationBar.scrollEdgeAppearance?.backgroundEffect = UIBlurEffect(style: .systemThickMaterial)
        navigationBar.compactScrollEdgeAppearance?.backgroundEffect = UIBlurEffect(style: .systemThickMaterial)
        toolbar.compactScrollEdgeAppearance?.backgroundEffect = UIBlurEffect(style: .systemThickMaterial)
        toolbar.scrollEdgeAppearance?.backgroundEffect = UIBlurEffect(style: .systemThickMaterial)
    }
    
    override init(nibName nibNameOrNil: String?, bundle nibBundleOrNil: Bundle?) {
        super.init(nibName: nibNameOrNil, bundle: nibBundleOrNil)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: ViewController
    override func viewDidLoad() {
        super.viewDidLoad()
        navigationBar.scrollEdgeAppearance = .init()
        navigationBar.scrollEdgeAppearance?.configureWithDefaultBackground()
        
        addKeyCommand(.close)
        GalleryManager.shared.addDelegate(self)
    }
    
    // MARK: Properties
    let gridVC = GalleryGridVC()
    
    var isShowingGrid: Bool {
        return topViewController == gridVC
    }
    
    var currentIndex: Int? {
        return (topViewController as? GalleryImagesVC)?.currentIndex
    }
    
    // MARK: Actions
    @objc func closeButtonTap() {
        dismiss(animated: true, completion: nil)
    }

    func openImage(at index: Int, animated: Bool) {
        if let imagesVC = topViewController as? GalleryImagesVC {
            imagesVC.openImage(at: index, animated: animated)
        }
        else {
            let imagesVC = GalleryImagesVC()
            imagesVC.initialIndex = index
            pushViewController(imagesVC, animated: animated)
        }
    }
}

extension GalleryNC : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        if items.isEmpty && isModal {
            dismiss(animated: true, completion: nil)
            return
        }
    }

}

#endif
