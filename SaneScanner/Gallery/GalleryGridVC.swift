//
//  GalleryGridVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

#if !targetEnvironment(macCatalyst)
import UIKit

class GalleryGridVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .background

        emptyStateView.backgroundColor = .background
        emptyStateLabel.adjustsFontForContentSizeCategory = true
        
        collectionViewLayout.maxSize = 320/3
        collectionViewLayout.margin = 6
        collectionViewLayout.linesHorizontalInset = 16
        collectionViewLayout.cellZIndex = 20 // default header zIndex is 10
        collectionView.contentInset.top = 20

        collectionView.backgroundColor = .background
        collectionView.allowsSelection = true
        collectionView.contentInsetAdjustmentBehavior = .always
        if #available(iOS 14.0, *) {
            // allow drag selection on iOS 14+
            collectionView.allowsMultipleSelectionDuringEditing = true
        }
        collectionView.collectionViewLayout = collectionViewLayout
        collectionView.registerSupplementaryView(GalleryGridHeader.self, kind: UICollectionView.elementKindSectionHeader, xib: false)
        collectionView.registerCell(GalleryThumbnailCell.self, xib: false)

        toolbarItems = [
            UIBarButtonItem.delete(target: self, action: #selector(self.deleteButtonTap(sender:))),
            UIBarButtonItem.flexibleSpace,
            UIBarButtonItem(title: "PDF", style: .plain, target: self, action: #selector(self.pdfButtonTap(sender:))),
            UIBarButtonItem.flexibleSpace,
            UIBarButtonItem.share(target: self, action: #selector(self.shareButtonTap(sender:)))
        ]
        
        GalleryManager.shared.addDelegate(self)
        
        updateNavBarContent(animated: false)
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        updateNavBarContent(animated: false)
        updateToolbarVisibility(animated: false)
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        // iOS automatically sets the toolbar visible in viewDidAppear, which could happen if we scroll to top quickly
        // and trigger the dismissal gesture just enough for it to trigger this method. let's make sure the toolbar is
        // always as visible as we'd like
        updateToolbarVisibility(animated: false)
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        if isEditing {
            setEditing(false, animated: false)
        }
    }
    
    // MARK: Properties
    private var hasInitiallyScrolledToBottom = false
    private var galleryItems = [[GalleryItem]]()
    
    private func setGalleryItems(_ items: [[GalleryItem]], reloadCollectionView: Bool = true) {
        self.galleryItems = items
        updateEmptyState()
        if reloadCollectionView {
            collectionView.reloadData()
        }
    }
    
    override func setEditing(_ editing: Bool, animated: Bool) {
        super.setEditing(editing, animated: animated)
        if #available(iOS 13.0, *) {
            // prevent dismissal of this view as a modal when editing
            isModalInPresentation = editing
        }
        updateNavBarContent(animated: animated)
        updateToolbarVisibility(animated: animated)
        
        collectionView.visibleCells.forEach {
            ($0 as? GalleryThumbnailCell)?.showSelectionIndicator = editing
        }
        
        collectionView.allowsMultipleSelection = editing
        if #available(iOS 14.0, *) {
            collectionView.isEditing = editing
        }
        
        if !editing {
            collectionView.indexPathsForSelectedItems?.forEach { collectionView.deselectItem(at: $0, animated: false) }
        }
    }

    // MARK: Views
    @IBOutlet private var emptyStateView: UIView!
    @IBOutlet private var emptyStateLabel: UILabel!
    private let collectionViewLayout = GalleryGridLayout()
    @IBOutlet private var collectionView: UICollectionView!
    private weak var hud: HUDAlertController?
    
    // MARK: Actions
    #if DEBUG
    @objc private func addTestImagesButtonTap() {
        GalleryManager.shared.createRandomTestImages(count: 10)
    }
    #endif
    
    private func openGallery(at index: Int) {
        let imagesVC = GalleryImagesVC()
        imagesVC.initialIndex = index
        navigationController?.pushViewController(imagesVC, animated: true)
    }
    
    @objc private func deleteButtonTap(sender: UIBarButtonItem) {
        guard let selectedIndices = collectionView.indexPathsForSelectedItems, !selectedIndices.isEmpty else { return }
        
        // keep a ref to the items, since we'll be deleting one by one and thus indices may become invalid, ending up deleting the wrong files
        let selectedItems = selectedIndices.map { self.galleryItems[$0] }
        
        var title = "DIALOG DELETE SCAN TITLE".localized
        var message = "DIALOG DELETE SCAN MESSAGE".localized
        
        if selectedItems.count > 1 {
            title   = "DIALOG DELETE SCANS TITLE".localized
            message = String(format: "DIALOG DELETE SCANS MESSAGE %d".localized, selectedItems.count)
        }
        
        let alert = UIAlertController(title: title, message: message, preferredStyle: .actionSheet)
        alert.addAction(UIAlertAction(title: "ACTION DELETE".localized, style: .destructive, handler: { (_) in
            self.hud = HUDAlertController.show(in: self)
            selectedItems.forEach { (item) in
                GalleryManager.shared.deleteItem(item)
            }
            self.hud?.dismiss(animated: true, completion: nil)
            self.setEditing(false, animated: true)
        }))
        alert.addAction(UIAlertAction(title: "ACTION CANCEL".localized, style: .cancel, handler: nil))
        alert.popoverPresentationController?.barButtonItem = sender
        present(alert, animated: true, completion: nil)
    }
    
    @objc private func pdfButtonTap(sender: UIBarButtonItem) {
        guard let selected = collectionView.indexPathsForSelectedItems, !selected.isEmpty else { return }
        
        hud = HUDAlertController.show(in: self)
        
        // needed to let HUD appear, even if PDF is generated on main thread
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
            self.shareSelectedItemsAsPDF(sender: sender)
        }
    }
    
    @objc private func shareButtonTap(sender: UIBarButtonItem) {
        let urls = self.selectedURLs()
        guard !urls.isEmpty else { return }
        
        UIActivityViewController.showForURLs(urls, from: sender, presentingVC: self, completion: nil)
    }
    
    // MARK: Sharing
    private func selectedURLs() -> [URL] {
        return (collectionView.indexPathsForSelectedItems ?? [])
            .sorted()
            .compactMap { self.galleryItems[$0].url }
    }
    
    private func shareSelectedItemsAsPDF(sender: UIBarButtonItem) {
        let urls = self.selectedURLs()
        guard !urls.isEmpty else { return }
        
        let tempURL = GalleryManager.shared.tempPdfFileUrl()
        
        do {
            try PDFGenerator.generatePDF(destination: tempURL, images: selectedURLs(), pageSize: Preferences.shared.pdfSize)
        }
        catch {
            HUDAlertController.dismiss(hud) {
                UIAlertController.show(for: error, in: self)
            }
            return
        }
        
        HUDAlertController.dismiss(hud) {
            UIActivityViewController.showForURLs([tempURL], from: sender, presentingVC: self) {
                // is called when the interaction with the PDF is done. It's either been copied, imported,
                // displayed, shared or printed, but we can dispose of it
                GalleryManager.shared.deleteTempPDF()
            }
        }
    }
    
    @objc private func editButtonTap() {
        setEditing(!isEditing, animated: true)
    }

    @objc private func closeButtonTap() {
        dismiss(animated: true, completion: nil)
    }
    
    // MARK: Content
    private func updateEmptyState() {
        let text = NSMutableAttributedString()
        text.append("GALLERY EMPTY TITLE".localized, font: .preferredFont(forTextStyle: .body), color: .normalText)
        text.append("\n\n", font: .preferredFont(forTextStyle: .subheadline), color: .normalText)
        text.append("GALLERY EMPTY SUBTITLE".localized, font: .preferredFont(forTextStyle: .subheadline), color: .altText)
        emptyStateLabel.attributedText = text
        
        emptyStateView.isHidden = !galleryItems.isEmpty
        collectionView.isHidden = galleryItems.isEmpty
    }
    
    private func updateNavBarContent(animated: Bool) {
        // Title
        if isEditing {
            let selectedCount = collectionView.indexPathsForSelectedItems?.count ?? 0
            title = "GALLERY SELECTED ITEMS COUNT %d".localized(quantity: selectedCount)
        } else {
            title = "GALLERY OVERVIEW TITLE".localized
        }
        
        // Left
        if navigationController?.isModal == true && !isEditing {
            navigationItem.setLeftBarButton(.done(target: self, action: #selector(self.closeButtonTap)), animated: animated)
        } else {
            #if DEBUG
            let testButton = UIBarButtonItem(title: "Add test images", style: .plain, target: self, action: #selector(self.addTestImagesButtonTap))
            navigationItem.setLeftBarButton(testButton, animated: animated)
            #else
            navigationItem.setLeftBarButton(nil, animated: animated)
            #endif
        }
        
        // Right
        var buttons = [UIBarButtonItem]()
        if isEditing {
            buttons.append(.done(target: self, action: #selector(self.editButtonTap)))
        } else {
            buttons.append(.edit(target: self, action: #selector(self.editButtonTap)))
        }
        
        navigationItem.setRightBarButtonItems(buttons, animated: animated)
    }
    
    private func updateToolbarVisibility(animated: Bool) {
        navigationController?.toolbar.alpha = isEditing ? 1 : 0.001
    }
    
    // MARK: Layout
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        collectionView.layoutIfNeeded()

        if !hasInitiallyScrolledToBottom, let lastIndexPath = galleryItems.lastIndexPath {
            hasInitiallyScrolledToBottom = true
            collectionView.scrollToItem(at: lastIndexPath, at: .bottom, animated: false)
        }
    }
    
    override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
        super.traitCollectionDidChange(previousTraitCollection)
        if previousTraitCollection?.preferredContentSizeCategory != traitCollection.preferredContentSizeCategory {
            collectionView.collectionViewLayout.invalidateLayout()
        }
    }
}

extension GalleryGridVC : UICollectionViewDataSource {
    func numberOfSections(in collectionView: UICollectionView) -> Int {
        return galleryItems.count
    }

    func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
        return galleryItems[section].count
    }
    
    func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueCell(GalleryThumbnailCell.self, for: indexPath)
        cell.update(item: galleryItems[indexPath], displayedOverTint: false)
        cell.showSelectionIndicator = isEditing
        return cell
    }
    
    func collectionView(_ collectionView: UICollectionView, viewForSupplementaryElementOfKind kind: String, at indexPath: IndexPath) -> UICollectionReusableView {
        let header = collectionView.dequeueSupplementaryView(GalleryGridHeader.self, kind: UICollectionView.elementKindSectionHeader, for: indexPath)
        header.items = galleryItems[indexPath.section]
        return header
    }
}

extension GalleryGridVC : UICollectionViewDelegateFlowLayout {
    func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, insetForSectionAt section: Int) -> UIEdgeInsets {
        let isLast = section == galleryItems.count - 1
        return UIEdgeInsets(
            top: 0,                  left:  self.collectionViewLayout.linesHorizontalInset,
            bottom: isLast ? 0 : 20, right: self.collectionViewLayout.linesHorizontalInset
        )
    }

    func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, referenceSizeForHeaderInSection section: Int) -> CGSize {
        return GalleryGridHeader.size(for: galleryItems[section], in: collectionView)
    }
    
    func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
        if isEditing {
            updateNavBarContent(animated: false)
        }
        else {
            collectionView.deselectItem(at: indexPath, animated: true)
            openGallery(at: indexPath.item)
        }
    }

    @available(iOS 13.0, *)
    func collectionView(_ collectionView: UICollectionView, contextMenuConfigurationForItemAt indexPath: IndexPath, point: CGPoint) -> UIContextMenuConfiguration? {
        let item = galleryItems[indexPath]
        let configuration = UIContextMenuConfiguration(
            identifier: nil,
            previewProvider: {
                return GalleryImagePreviewVC(item: item)
            },
            actionProvider: { _ in
                let open = UIAction(title: "ACTION OPEN".localized, image: UIImage(systemName: "folder")) { _ in
                    self.openGallery(at: indexPath.item)
                }
                let share = UIAction(title: "ACTION SHARE".localized, image: UIImage(systemName: "square.and.arrow.up")) { _ in
                    UIActivityViewController.showForURLs([item.url], in: self, sender: collectionView.cellForItem(at: indexPath), completion: nil)
                }
                return UIMenu(title: item.suggestedDescription(separator: "\n") ?? "", children: [open, share])
            }
        )
        configuration.indexPath = indexPath
        return configuration
    }

    @available(iOS 13.0, *)
    func collectionView(_ collectionView: UICollectionView, willPerformPreviewActionForMenuWith configuration: UIContextMenuConfiguration, animator: UIContextMenuInteractionCommitAnimating) {
        guard let indexPath = configuration.indexPath else { return }
        animator.addCompletion {
            self.openGallery(at: indexPath.item)
        }
    }
    
    func collectionView(_ collectionView: UICollectionView, shouldBeginMultipleSelectionInteractionAt indexPath: IndexPath) -> Bool {
        return true
    }
    
    func collectionView(_ collectionView: UICollectionView, didBeginMultipleSelectionInteractionAt indexPath: IndexPath) {
        setEditing(true, animated: true)
    }
}

extension GalleryGridVC : UICollectionViewDragDelegate {
    func collectionView(_ collectionView: UICollectionView, itemsForBeginning session: UIDragSession, at indexPath: IndexPath) -> [UIDragItem] {
        let item = galleryItems[indexPath]
        return [UIDragItem(itemProvider: NSItemProvider(object: item))]
    }
}

extension GalleryGridVC : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        let groups = manager.groupedItems
        guard groups.count == galleryItems.count else {
            // number of groups differs, let's not do any incremental updates
            setGalleryItems(groups)
            return
        }
        
        let newIndexedGroups = groups.allItems
        let oldIndexedGroups = galleryItems.allItems

        let addedIndexPaths: [IndexPath] = newItems
            .compactMap { newItem in newIndexedGroups.first(where: { $0.1 == newItem })?.0 }
        let removedIndexPaths: [IndexPath] = removedItems
            .compactMap { removedItem in oldIndexedGroups.first(where: { $0.1 == removedItem })?.0 }
        
        // if there are only additions, and we can find all items that have been added, then do an animated update
        if newItems.isNotEmpty && removedItems.isEmpty, addedIndexPaths.count == newItems.count {
            collectionView.performBatchUpdates({
                setGalleryItems(groups, reloadCollectionView: false)
                collectionView.insertItems(at: addedIndexPaths)
            }, completion: nil)
            return
        }

        // if there are only deletions, and we can find all items that have been deleted, then do an animated update
        if removedItems.isNotEmpty && newItems.isEmpty, removedIndexPaths.count == removedItems.count {
            collectionView.performBatchUpdates({
                setGalleryItems(groups, reloadCollectionView: false)
                collectionView.deleteItems(at: removedIndexPaths)
            }, completion: nil)
            return
        }
        
        setGalleryItems(groups)
    }
}
#endif
