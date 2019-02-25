//
//  GalleryImagesVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

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
    
    @available(iOS 11.0, *)
    override var prefersHomeIndicatorAutoHidden: Bool {
        return navigationController?.isNavigationBarHidden ?? super.prefersHomeIndicatorAutoHidden
    }
    
    // MARK: Views
    private let pagesVC = UIPageViewController(transitionStyle: .scroll, navigationOrientation: .horizontal, options: nil)
    
    // MARK: Actions
    func openImage(at index: Int, animated: Bool) {
        guard index != currentIndex else { return }
        guard let vc = imageViewController(at: index) else { return }
        
        let isAfterCurrent = index > (currentIndex ?? 0)
        currentIndex = index
        
        pagesVC.setViewControllers([vc], direction: isAfterCurrent ? .forward : .reverse, animated: animated, completion: nil)
    }
    
    @objc private func closeButtonTap() {
        dismiss(animated: true, completion: nil)
    }
    
    @objc private func deleteCurrentImage() {
        // TODO: prevent when scrolling
        // guard !imageViewerViewController.isUserScrolling else { return }
        
        guard let currentIndex = currentIndex else { return }
        let item = items[currentIndex]
        
        DLAVAlertView(
            title: "DIALOG TITLE DELETE SCAN".localized,
            message: "DIALOG MESSAGE DELETE SCAN".localized,
            delegate: nil,
            cancel: "ACTION CANCEL".localized,
            others: ["ACTION DELETE".localized])
            .show { (alert, index) in
                guard index != alert?.cancelButtonIndex else { return }
                
                if self.items.count == 1 {
                    self.dismiss(animated: true, completion: nil)
                }
                
                GalleryManager.shared.deleteItem(item)
        }
    }
    
    @objc private func shareCurrentImage(sender: UIBarButtonItem) {
        // TODO: prevent when scrolling
        // guard !imageViewerViewController.isUserScrolling else { return }
        
        guard let currentIndex = currentIndex else { return }
        let item = items[currentIndex]

        UIActivityViewController.showForURLs([item.URL], from: sender, presentingVC: self, completion: nil)
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
        
        let canHideNavBars = navigationController?.sy_isModal ?? false
        let hideNavBars = !showNavBars && canHideNavBars
        
        navigationController?.isNavigationBarHidden = hideNavBars
        navigationController?.isToolbarHidden = hideNavBars
        view.backgroundColor = hideNavBars ? .black : .groupTableViewBackground
        
        setNeedsStatusBarAppearanceUpdate()
        if #available(iOS 11.0, *) {
            setNeedsUpdateOfHomeIndicatorAutoHidden()
        }
    }
    
    private func updateNavBarContent() {
        title = String(format: "GALLERY IMAGE %d OF %d".localized, (currentIndex ?? 0) + 1, items.count)
        
        if navigationController?.sy_isModal == true {
            navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(self.closeButtonTap))
            navigationItem.rightBarButtonItem?.style = .done
        } else {
            navigationItem.rightBarButtonItem = nil
        }
    }
    
    private func updateToolbarContent() {
        guard let currentIndex = currentIndex else {
            toolbarItems = nil
            return
        }
        
        toolbarItems = [
            UIBarButtonItem(barButtonSystemItem: .trash, target: self, action: #selector(self.deleteCurrentImage)),
            UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil),
            UIBarButtonItem(title: GalleryManager.shared.dateString(for: items[currentIndex]), style: .plain, target: nil, action: nil),
            UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil),
            UIBarButtonItem(barButtonSystemItem: .action, target: self, action: #selector(self.shareCurrentImage(sender:))),
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
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        let prevItems = self.items
        self.items = items
        
        guard let prevIndex = currentIndex, !prevItems.isEmpty, !items.isEmpty else { return }
        let prevItem = prevItems[prevIndex]

        if removedItems.contains(prevItem) {
            openImage(at: max(0, prevIndex - 1), animated: true)
            return
        }
        
        guard let newIndex = items.firstIndex(of: prevItem) else { return }
        openImage(at: newIndex, animated: false)
    }
}
