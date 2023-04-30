//
//  GalleryImagesVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

#if !targetEnvironment(macCatalyst)
class GalleryImagesVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        pagesVC.dataSource = self
        pagesVC.delegate = self
        
        pagesVC.willMove(toParent: self)
        addChild(pagesVC)
        view.addSubview(pagesVC.view)
        pagesVC.view.snp.makeConstraints { (make) in
            make.edges.equalToSuperview()
        }
        pagesVC.didMove(toParent: self)

        GalleryManager.shared.addDelegate(self)
        
        openImage(at: initialIndex ?? 0, animated: false)
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        updateNavBarContent()
        updateToolbarContent()
        updateDisplayMode(showNavBars: true, animated: false)
    }
    
    // MARK: Properties
    var initialIndex: Int?
    private var items = [GalleryItem]()
    private(set) var currentIndex: Int?  {
        didSet {
            updateNavBarContent()
            updateToolbarContent()
        }
    }
    
    override var prefersStatusBarHidden: Bool {
        return navigationController?.isNavigationBarHidden ?? super.prefersStatusBarHidden
    }
    
    override var prefersHomeIndicatorAutoHidden: Bool {
        return navigationController?.isNavigationBarHidden ?? super.prefersHomeIndicatorAutoHidden
    }
    
    // MARK: Views
    private let pagesVC = UIPageViewController(transitionStyle: .scroll, navigationOrientation: .horizontal, options: nil)
    
    // MARK: Actions
    func openImage(at index: Int, animated: Bool) {
        // don't ignore if index == currentIndex, because the image list may have changed (for instance when deleting
        // the first image) and we still need to show the new page

        guard let vc = imageViewController(at: index) else { return }
        
        let isAfterCurrent = index >= (currentIndex ?? 0)
        currentIndex = index
        
        pagesVC.setViewControllers([vc], direction: isAfterCurrent ? .forward : .reverse, animated: animated, completion: nil)
    }
    
    @objc private func closeButtonTap() {
        dismiss(animated: true, completion: nil)
    }
    
    @objc private func deleteCurrentImage(sender: UIBarButtonItem) {
        guard let currentIndex = currentIndex else { return }
        let item = items[currentIndex]
        
        let alert = UIAlertController(title: "DIALOG DELETE SCAN TITLE".localized, message: "DIALOG DELETE SCAN MESSAGE".localized, preferredStyle: .actionSheet)
        alert.addAction(UIAlertAction(title: "ACTION DELETE".localized, style: .destructive, handler: { (_) in
            if self.items.count == 1 {
                self.dismiss(animated: true, completion: nil)
            }
            
            GalleryManager.shared.deleteItem(item)
        }))
        alert.addAction(UIAlertAction(title: "ACTION CANCEL".localized, style: .cancel, handler: nil))
        alert.popoverPresentationController?.barButtonItem = sender
        present(alert, animated: true, completion: nil)
    }
    
    @objc private func shareCurrentImage(sender: UIBarButtonItem) {
        guard let currentIndex = currentIndex else { return }
        let item = items[currentIndex]

        UIActivityViewController.showForURLs([item.url], from: sender, presentingVC: self)
    }

    // MARK: Content
    private func imageViewController(at index: Int) -> GalleryImageVC? {
        guard index >= 0, index < items.count else { return nil }
        let vc = GalleryImageVC(item: items[index], index: index)
        vc.delegate = self
        return vc
    }
    
    private func toggleDisplayMode(animated: Bool) {
        let navBarsCurrentlyHidden = navigationController?.isNavigationBarHidden ?? false
        updateDisplayMode(showNavBars: navBarsCurrentlyHidden, animated: animated)
    }
    
    private func updateDisplayMode(showNavBars: Bool, animated: Bool) {
        if animated {
            UIView.transition(with: view.window ?? view, duration: 0.3, options: .transitionCrossDissolve, animations: {
                self.updateDisplayMode(showNavBars: showNavBars, animated: false)
            }, completion: nil)
            return
        }
        
        let canHideNavBars = navigationController?.isModal ?? false
        let hideNavBars = !showNavBars && canHideNavBars
        
        navigationController?.isNavigationBarHidden = hideNavBars
        navigationController?.isToolbarHidden = hideNavBars
        view.backgroundColor = hideNavBars ? .black : .background
        
        setNeedsStatusBarAppearanceUpdate()
        setNeedsUpdateOfHomeIndicatorAutoHidden()
    }
    
    private func updateNavBarContent() {
        title = "GALLERY ITEM %d OF %d".localized((currentIndex ?? 0) + 1, items.count)
        
        if navigationController?.isModal == true {
            navigationItem.rightBarButtonItem = .close(target: self, action: #selector(self.closeButtonTap))
        } else {
            navigationItem.rightBarButtonItem = nil
        }
    }
    
    private func updateToolbarContent() {
        guard let currentIndex = currentIndex else {
            toolbarItems = nil
            return
        }
        
        let titleButton = UIBarButtonItem(
            title: items[currentIndex].creationDateString(includingTime: true, allowRelative: true),
            style: .plain, target: nil, action: nil
        )
        titleButton.tintColor = .normalText
        
        toolbarItems = [
            UIBarButtonItem.share(target: self, action: #selector(self.shareCurrentImage(sender:))),
            UIBarButtonItem.flexibleSpace,
            titleButton,
            UIBarButtonItem.flexibleSpace,
            UIBarButtonItem.delete(target: self, action: #selector(self.deleteCurrentImage(sender:))),
        ]
    }
}

extension GalleryImagesVC : UIPageViewControllerDataSource {
    func pageViewController(_ pageViewController: UIPageViewController, viewControllerBefore viewController: UIViewController) -> UIViewController? {
        guard let currentVC = viewController as? GalleryImageVC else { return nil }
        return imageViewController(at: currentVC.galleryIndex - 1)
    }
    
    func pageViewController(_ pageViewController: UIPageViewController, viewControllerAfter viewController: UIViewController) -> UIViewController? {
        guard let currentVC = viewController as? GalleryImageVC else { return nil }
        return imageViewController(at: currentVC.galleryIndex + 1)
    }
}

extension GalleryImagesVC : UIPageViewControllerDelegate {
    func pageViewController(_ pageViewController: UIPageViewController, didFinishAnimating finished: Bool, previousViewControllers: [UIViewController], transitionCompleted completed: Bool) {
        if completed {
            currentIndex = (pageViewController.viewControllers?.first as? GalleryImageVC)?.galleryIndex
        }
    }
}

extension GalleryImagesVC : GalleryImageVCDelegate {
    func galleryImageVC(singleTapped: GalleryImageVC) {
        toggleDisplayMode(animated: true)
    }
}

extension GalleryImagesVC : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        let prevItems = self.items
        self.items = items
        
        guard let prevIndex = currentIndex, !prevItems.isEmpty, !items.isEmpty else { return }
        let prevItem = prevItems[prevIndex]

        if removedItems.contains(prevItem) {
            // if deleting last: show the image before the one that we deleted
            // else: reload the current index (visually this will look like going to the next one)
            let newIndex = prevIndex >= items.count ? max(0, prevIndex - 1) : prevIndex
            openImage(at: newIndex, animated: true)
            return
        }
        
        guard let newIndex = items.firstIndex(of: prevItem) else { return }
        openImage(at: newIndex, animated: false)
    }
}

#endif
