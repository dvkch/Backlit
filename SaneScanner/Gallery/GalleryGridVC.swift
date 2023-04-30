//
//  GalleryGridVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 24/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

#if !targetEnvironment(macCatalyst)
import UIKit
import DiffableDataSources

class GalleryGridVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .background
        navigationController?.navigationBar.setBackButtonImage(.icon(.left))

        emptyStateView.backgroundColor = .background
        emptyStateLabel.adjustsFontForContentSizeCategory = true
        
        collectionViewLayout.maxSize = 320/3
        collectionViewLayout.margin = 6
        collectionViewLayout.linesHorizontalInset = 16
        collectionViewLayout.cellZIndex = 20 // default header zIndex is 10

        collectionView.dataSource = galleryDataSource
        collectionView.backgroundColor = .background
        collectionView.allowsSelection = true
        collectionView.contentInsetAdjustmentBehavior = .never
        if #available(iOS 13.0, *) {
            collectionView.automaticallyAdjustsScrollIndicatorInsets = false
        }
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
    
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        if isEditing {
            setEditing(false, animated: false)
        }
    }
    
    // MARK: Properties
    private var hasInitiallyScrolledToBottom = false
    private lazy var galleryDataSource = CollectionViewDiffableDataSource<GalleryGroup, GalleryItem>(collectionView: collectionView, viewsProvider: self)
    
    private func updateDataSource(using groups: [GalleryGroup], animated: Bool) {
        var snapshot = DiffableDataSourceSnapshot<GalleryGroup, GalleryItem>()
        groups.forEach { group in
            snapshot.appendSections([group])
            snapshot.appendItems(group.items, toSection: group)
        }
        galleryDataSource.apply(snapshot, animatingDifferences: animated)
        collectionView.reloadVisibleSectionHeaders { (indexPath: IndexPath, header: GalleryGridHeader) in
            header.group = groups[indexPath.section]
        }

        updateEmptyState()
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
    
    private func openGallery(on item: GalleryItem) {
        let imagesVC = GalleryImagesVC()
        imagesVC.initialIndex = GalleryManager.shared.galleryItems.firstIndex(of: item)
        navigationController?.pushViewController(imagesVC, animated: true)
    }
    
    @objc private func deleteButtonTap(sender: UIBarButtonItem) {
        guard let selectedIndices = collectionView.indexPathsForSelectedItems, !selectedIndices.isEmpty else { return }
        
        // keep a ref to the items, since we'll be deleting one by one and thus indices may become invalid, ending up deleting the wrong files
        let selectedItems = selectedIndices.compactMap { self.galleryDataSource.itemIdentifier(for: $0) }
        
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
            .compactMap { self.galleryDataSource.itemIdentifier(for: $0)?.url }
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
        
        emptyStateView.isHidden = galleryDataSource.totalCount > 0
        collectionView.isHidden = !emptyStateView.isHidden
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
            navigationItem.setLeftBarButton(.edit(target: self, action: #selector(self.editButtonTap)), animated: animated)
        } else {
            navigationItem.setLeftBarButton(.done(target: self, action: #selector(self.editButtonTap)), animated: animated)
        }
        
        // Right
        if !isEditing {
            navigationItem.setRightBarButton(.close(target: self, action: #selector(self.closeButtonTap)), animated: animated)
        } else {
            navigationItem.setRightBarButton(nil, animated: animated)

            #if DEBUG && !targetEnvironment(simulator)
            let testButton = UIBarButtonItem(title: "Add test images", style: .plain, target: self, action: #selector(self.addTestImagesButtonTap))
            navigationItem.setRightBarButton(testButton, animated: animated)
            #endif
        }
    }
    
    private func updateToolbarVisibility(animated: Bool) {
        navigationController?.setToolbarHidden(!isEditing, animated: animated)
        view.setNeedsLayout()
    }
    
    // MARK: Layout
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()

        // we can't directly use `collectionView.contentInsetAdjustmentBehavior = .never`, or the bottom
        // offset will move around when the toolbar is shown/hidden, which is a problem for a view
        // who opens by default at the bottom...
        let bottomInsets = collectionViewLayout.linesHorizontalInset
        collectionView.contentInset = .init(
            top: view.safeAreaInsets.top + 20,
            left: view.safeAreaInsets.left,
            bottom: (view.window?.safeAreaInsets.bottom ?? 0) + (navigationController?.toolbar.bounds.height ?? 0) + bottomInsets,
            right: view.safeAreaInsets.right
        )
        collectionView.scrollIndicatorInsets = collectionView.contentInset
        collectionView.scrollIndicatorInsets.top -= 20
        collectionView.layoutIfNeeded()

        if !hasInitiallyScrolledToBottom, let lastIndexPath = galleryDataSource.lastIndexPath {
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

extension GalleryGridVC : CollectionViewDiffableDataSourceViewsProvider {
    typealias SectionIdentifier = GalleryGroup
    typealias ItemIdentifier = GalleryItem
    
    func collectionView(_ collectionView: UICollectionView, cellForItem item: GalleryItem, at indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueCell(GalleryThumbnailCell.self, for: indexPath)
        cell.update(item: item, displayedOverTint: false)
        cell.showSelectionIndicator = isEditing
        return cell
    }
    
    func collectionView(_ collectionView: UICollectionView, viewForSupplementaryElementOfKind kind: String, at indexPath: IndexPath) -> UICollectionReusableView {
        let header = collectionView.dequeueSupplementaryView(GalleryGridHeader.self, kind: UICollectionView.elementKindSectionHeader, for: indexPath)
        header.group = galleryDataSource.snapshot().sectionIdentifiers[indexPath.section]
        return header
    }
}

extension GalleryGridVC : UICollectionViewDelegateFlowLayout {
    func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, insetForSectionAt section: Int) -> UIEdgeInsets {
        let isLast = section == galleryDataSource.snapshot().numberOfSections - 1
        return UIEdgeInsets(
            top: 0,                  left:  self.collectionViewLayout.linesHorizontalInset,
            bottom: isLast ? 0 : 20, right: self.collectionViewLayout.linesHorizontalInset
        )
    }

    func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, referenceSizeForHeaderInSection section: Int) -> CGSize {
        return GalleryGridHeader.size(for: galleryDataSource.snapshot().sectionIdentifiers[section], in: collectionView)
    }
    
    func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
        if isEditing {
            updateNavBarContent(animated: false)
        }
        else {
            collectionView.deselectItem(at: indexPath, animated: true)
            if let item = galleryDataSource.itemIdentifier(for: indexPath) {
                openGallery(on: item)
            }
        }
    }

    @available(iOS 13.0, *)
    func collectionView(_ collectionView: UICollectionView, contextMenuConfigurationForItemAt indexPath: IndexPath, point: CGPoint) -> UIContextMenuConfiguration? {
        guard let item = galleryDataSource.itemIdentifier(for: indexPath) else { return nil }
        let configuration = UIContextMenuConfiguration(
            identifier: nil,
            previewProvider: {
                return GalleryImagePreviewVC(item: item)
            },
            actionProvider: { _ in
                let open = UIAction(title: "ACTION OPEN".localized, image: UIImage(systemName: "folder")) { _ in
                    self.openGallery(on: item)
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
        guard let indexPath = configuration.indexPath, let item = galleryDataSource.itemIdentifier(for: indexPath) else { return }
        animator.addCompletion {
            self.openGallery(on: item)
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
        guard let item = galleryDataSource.itemIdentifier(for: indexPath) else { return [] }
        return [UIDragItem(itemProvider: NSItemProvider(object: item))]
    }
}

extension GalleryGridVC : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        updateDataSource(using: manager.galleryGroups, animated: true)
    }
}
#endif
