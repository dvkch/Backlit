//
//  GalleryNC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit

class GalleryNC: UINavigationController {

    // MARK: Init
    init(openedAt index: Int?) {
        super.init(rootViewController: gridVC)
        
        if let index = index {
            let imagesVC = GalleryImagesVC()
            imagesVC.initialIndex = index
            self.viewControllers = [gridVC, imagesVC]
        }
        else {
            self.viewControllers = [gridVC]
        }
        
        // make sure the back button has the right name when opening at specific index
        gridVC.loadViewIfNeeded()
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
    func openGallery(animated: Bool) {
        popToRootViewController(animated: animated)
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
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        if items.isEmpty && sy_isModal {
            dismiss(animated: true, completion: nil)
            return
        }
        
        /*
        if !isShowingGrid {
            let currentIndex = imageViewerViewController.pageIndex
            var index: Int?
            var adding = false
            
            if let newItem = newItems.first {
                index = items.index(of: newItem)
            }
            
            if let removedItem = removedItems.first {
                index = items.index(of: removedItem)
                adding = false
            }
            
            if index == currentIndex {
                let transition = CATransition()
                transition.duration = 0.3
                transition.timingFunction = CAMediaTimingFunction(name: .easeInEaseOut)
                transition.type = .push
                transition.subtype = .fromRight
                
                if adding || index == self.galleryItems.count - 1 {
                    transition.subtype = .fromLeft
                }
                
                imageViewerViewController.pageViewController.view.layer.add(transition, forKey: kCATransition)
            }
        }*/
    }

}