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
        emptyStateLabel.autoAdjustsFontSize = true
        
        collectionViewLayout.maxSize = 320/3
        collectionViewLayout.margin = 2

        collectionView.backgroundColor = .background
        collectionView.allowsMultipleSelection = true // requires holding option on macOS
        collectionView.collectionViewLayout = collectionViewLayout
        collectionView.registerCell(GalleryThumbnailCell.self, xib: false)
        
        toolbarItems = [
            UIBarButtonItem(barButtonSystemItem: .trash, target: self, action: #selector(self.deleteButtonTap(sender:))),
            UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil),
            UIBarButtonItem(title: "PDF", style: .plain, target: self, action: #selector(self.pdfButtonTap(sender:))),
            UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil),
            UIBarButtonItem(barButtonSystemItem: .action, target: self, action: #selector(self.shareButtonTap(sender:)))
        ]
        
        GalleryManager.shared.addDelegate(self)
        
        updateNavBarContent(animated: false)
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        updateNavBarContent(animated: false)
        updateToolbarVisibility(animated: false)
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        if isEditing {
            setEditing(false, animated: false)
        }
    }
    
    // MARK: Properties
    private var items = [GalleryItem]()
    
    private func setItems(_ items: [GalleryItem], reloadCollectionView: Bool = true) {
        self.items = items
        updateEmptyState()
        if reloadCollectionView {
            collectionView.reloadData()
        }
    }
    
    override func setEditing(_ editing: Bool, animated: Bool) {
        super.setEditing(editing, animated: animated)
        updateNavBarContent(animated: animated)
        updateToolbarVisibility(animated: animated)
        
        collectionView.visibleCells.forEach {
            ($0 as? GalleryThumbnailCell)?.showSelectionIndicator = editing
        }
        
        collectionView.allowsMultipleSelection = editing
        
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
        let selectedItems = selectedIndices.map { self.items[$0.item] }
        
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
            .reversed()
            .compactMap { self.items[$0.row].url }
    }
    
    private func shareSelectedItemsAsPDF(sender: UIBarButtonItem) {
        let urls = self.selectedURLs()
        guard !urls.isEmpty else { return }
        
        let tempURL = GalleryManager.shared.tempPdfFileUrl()
        
        do {
            try PDFGenerator.generatePDF(destination: tempURL, images: selectedURLs(), aspectRatio: 210 / 297, jpegQuality: 0.9, fixedPageSize: true)
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
        
        emptyStateView.isHidden = !items.isEmpty
        collectionView.isHidden = items.isEmpty
    }
    
    private func updateNavBarContent(animated: Bool) {
        // Title
        if isEditing {
            let selectedCount = collectionView.indexPathsForSelectedItems?.count ?? 0
            switch selectedCount {
            case 0:     title = "GALLERY OVERVIEW SELECTED NONE".localized
            case 1:     title = String(format: "GALLERY OVERVIEW SELECTED SINGULAR %d".localized, selectedCount)
            default:    title = String(format: "GALLERY OVERVIEW SELECTED PLURAL %d".localized, selectedCount)
            }
        } else {
            title = "GALLERY OVERVIEW TITLE".localized
        }
        
        // Left
        if navigationController?.sy_isModal == true && !isEditing {
            let closeButton = UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(self.closeButtonTap))
            closeButton.style = .done
            navigationItem.setLeftBarButton(closeButton, animated: animated)
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
            buttons.append(UIBarButtonItem(barButtonSystemItem: .done, target: self, action: #selector(self.editButtonTap)))
            buttons.last?.style = .done
        } else {
            buttons.append(UIBarButtonItem(barButtonSystemItem: .edit, target: self, action: #selector(self.editButtonTap)))
        }
        
        navigationItem.setRightBarButtonItems(buttons, animated: animated)
    }
    
    private func updateToolbarVisibility(animated: Bool) {
        navigationController?.setToolbarHidden(!isEditing, animated: animated)
    }
}

extension GalleryGridVC : UICollectionViewDataSource {
    func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
        return items.count
    }
    
    func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
        let cell = collectionView.dequeueCell(GalleryThumbnailCell.self, for: indexPath)
        cell.update(item: items[indexPath.row], mode: .gallery, spinnerColor: UIColor.gray)
        cell.showSelectionIndicator = isEditing
        return cell
    }
}

extension GalleryGridVC : UICollectionViewDelegate {
    func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
        if isEditing {
            updateNavBarContent(animated: false)
        }
        else {
            collectionView.deselectItem(at: indexPath, animated: true)
            openGallery(at: indexPath.item)
        }
    }

    #if !targetEnvironment(macCatalyst)
    @available(iOS 13.0, *)
    func collectionView(_ collectionView: UICollectionView, contextMenuConfigurationForItemAt indexPath: IndexPath, point: CGPoint) -> UIContextMenuConfiguration? {
        let configuration = UIContextMenuConfiguration(identifier: nil, previewProvider: nil) { _ in
            let open = UIAction(title: "ACTION OPEN".localized, image: UIImage(systemName: "folder")) { _ in
                self.openGallery(at: indexPath.item)
            }
            let share = UIAction(title: "ACTION SHARE".localized, image: UIImage(systemName: "square.and.arrow.up")) { _ in
                UIActivityViewController.showForURLs([self.items[indexPath.item].url], in: self, sender: collectionView.cellForItem(at: indexPath), completion: nil)
            }
            return UIMenu(title: "", children: [open, share])
        }
        return configuration
    }
    #endif
}

@available(iOS 11.0, *)
extension GalleryGridVC : UICollectionViewDragDelegate {
    func collectionView(_ collectionView: UICollectionView, itemsForBeginning session: UIDragSession, at indexPath: IndexPath) -> [UIDragItem] {
        let item = items[indexPath.item]
        return [UIDragItem(itemProvider: NSItemProvider(object: item))]
    }
}

extension GalleryGridVC : GalleryManagerDelegate {
    func galleryManager(_ manager: GalleryManager, didCreate thumbnail: UIImage, for item: GalleryItem) { }
    
    func galleryManager(_ manager: GalleryManager, didUpdate items: [GalleryItem], newItems: [GalleryItem], removedItems: [GalleryItem]) {
        let newIndices = newItems.compactMap { items.firstIndex(of: $0) }
        let removedIndices = removedItems.compactMap { self.items.firstIndex(of: $0) }
        
        // if there are only additions, and we can find all items that have been added, then do an animated update
        if removedItems.isEmpty, !newItems.isEmpty, newIndices.count == newItems.count {
            collectionView.performBatchUpdates({
                setItems(items, reloadCollectionView: false)
                collectionView.insertItems(at: newIndices.map { IndexPath(item: $0, section: 0) })
            }, completion: nil)
            return
        }

        // if there are only deletions, and we can find all items that have been deleted, then do an animated update
        if newItems.isEmpty, !removedItems.isEmpty, removedIndices.count == removedItems.count {
            collectionView.performBatchUpdates({
                setItems(items, reloadCollectionView: false)
                collectionView.deleteItems(at: removedIndices.map { IndexPath(item: $0, section: 0) })
            }, completion: nil)
            return
        }
        
        setItems(items)
    }
}
#endif
