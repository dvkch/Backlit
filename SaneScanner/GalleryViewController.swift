//
//  GalleryViewController.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import MHVideoPhotoGallery
import SYKit

class GalleryViewController: MHGalleryController {

    override init(presentationStyle: MHGalleryViewMode, uiCustomization: MHUICustomization) {
        super.init(presentationStyle: presentationStyle, uiCustomization: uiCustomization)

        self.overViewViewController = OverviewController()
        
        if presentationStyle != .overView {
            self.viewControllers = [overViewViewController, imageViewerViewController]
        } else {
            self.viewControllers = [overViewViewController]
        }
        
        transitionCustomization.dismissWithScrollGestureOnFirstAndLastImage = false
        galleryDelegate = self
        
        GalleryManager.shared.addDelegate(self)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Actions
    @objc private func deleteImage() {
        guard !imageViewerViewController.isUserScrolling else { return }
        
        let index = imageViewerViewController.pageIndex
        let item = dataSource.item(for: index)!
        
        DLAVAlertView(
            title: "DIALOG TITLE DELETE SCAN".localized,
            message: "DIALOG MESSAGE DELETE SCAN".localized,
            delegate: nil,
            cancel: "ACTION CANCEL".localized,
            others: ["ACTION DELETE".localized])
            .show { (alert, index) in
                guard index != alert?.cancelButtonIndex else { return }
                if self.dataSource.numberOfItems(inGallery: self) == 1 {
                    self.dismiss(animated: true, dismiss: self.imageViewerViewController.dismissFromImageView, completion: nil)
                }
            
                GalleryManager.shared.deleteItem(item)
        }
    }
    
    
    @objc private func shareImage(sender: UIBarButtonItem) {
        guard !imageViewerViewController.isUserScrolling else { return }

        let index = imageViewerViewController.pageIndex
        let item = dataSource.item(for: index)!
        
        UIActivityViewController.showForURLs([item.url], from: sender, presentingVC: self, completion: nil)
    }
}

extension GalleryViewController : MHGalleryDelegate {
    func customizeableToolBarItems(_ toolBarItems: [MHBarButtonItem]!, for galleryItem: MHGalleryItem!) -> [MHBarButtonItem]! {
        guard galleryItem.galleryType != .video else { return toolBarItems }
        
        let deleteButton = MHBarButtonItem(barButtonSystemItem: .trash, target: self, action: #selector(self.deleteImage))
        deleteButton.type = .custom
        
        let dateButton = MHBarButtonItem(title: GalleryManager.shared.dateString(for: galleryItem), style: .plain, target: nil, action: nil)
        dateButton.type = .custom
        
        let shareButton = MHBarButtonItem(barButtonSystemItem: .action, target: self, action: #selector(self.shareImage(sender:)))
        shareButton.type = .share
        
        let flexibleSpace = MHBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil)
        flexibleSpace.type = .flexible
        
        return [deleteButton, flexibleSpace, dateButton, flexibleSpace, shareButton]
    }
}

extension GalleryViewController : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [MHGalleryItem], newItems: [MHGalleryItem], removedItems: [MHGalleryItem]) {
        if items.isEmpty && sy_isModal {
            dismiss(animated: true, completion: nil)
            return
        }
        
        if !isShowingOverview() {
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
        }
        
        self.galleryItems = items
    }
}

