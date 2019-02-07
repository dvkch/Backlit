//
//  OverviewController.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SVProgressHUD
import MHVideoPhotoGallery

class OverviewController: MHOverviewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        
        // add trash and PDF buttons
        let trashButton   = UIBarButtonItem(barButtonSystemItem: .trash, target: self, action: #selector(self.deleteButtonTap))
        let pdfButton     = UIBarButtonItem(title: "PDF", style: .plain, target: self, action: #selector(self.pdfButtonTap(sender:)))
        let shareButton   = UIBarButtonItem(barButtonSystemItem: .action, target: self, action: #selector(self.shareButtonTap(sender:)))
        let flexibleSpace = UIBarButtonItem(barButtonSystemItem: .flexibleSpace, target: nil, action: nil)
        
        toolbarItems = [trashButton, flexibleSpace, pdfButton, flexibleSpace, shareButton]
    }
    
    
    // MARK: Actions
    @objc private func deleteButtonTap() {
        guard let selected = collectionView.indexPathsForSelectedItems, !selected.isEmpty else { return }
        
        var title = "DIALOG TITLE DELETE SCAN".localized
        var message = "DIALOG MESSAGE DELETE SCAN".localized
        
        if selected.count > 1 {
            title   = "DIALOG TITLE DELETE SCANS".localized
            message = String(format: "DIALOG MESSAGE DELETE SCANS %d".localized, selected.count)
        }
        
        DLAVAlertView(title: title, message: message, delegate: nil, cancel: "ACTION CANCEL".localized, others: ["ACTION DELETE".localized])
            .show { (alert, index) in
                guard index != alert?.cancelButtonIndex else { return }
                
                SVProgressHUD.show()
                selected.forEach { (indexPath) in
                    GalleryManager.shared.deleteItem(self.item(for: indexPath.item))
                }
                SVProgressHUD.dismiss()
                self.setEditing(false, animated: true)
            }
    }
    
    @objc private func pdfButtonTap(sender: UIBarButtonItem) {
        guard let selected = collectionView.indexPathsForSelectedItems, !selected.isEmpty else { return }

        SVProgressHUD.show()
        
        // needed to let SVProgressHUD appear, even if PDF is generated on main thread
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
            .compactMap { self.item(for: $0.item)?.url }
    }
    
    private func shareSelectedItemsAsPDF(sender: UIBarButtonItem) {
        let urls = self.selectedURLs()
        guard !urls.isEmpty else { return }

        let tempURL = GalleryManager.shared.tempPdfFileUrl()
        
        do {
            try PDFGenerator.generatePDF(destination: tempURL, images: selectedURLs(), aspectRatio: 210 / 297, jpegQuality: 0.9, fixedPageSize: true)
            SVProgressHUD.dismiss()
        }
        catch {
            SVProgressHUD.showError(withStatus: error.localizedDescription)
            return
        }

        UIActivityViewController.showForURLs([tempURL], from: sender, presentingVC: self) {
            // is called when the interaction with the PDF is done. It's either been copied, imported,
            // displayed, shared or printed, but we can dispose of it
            GalleryManager.shared.deleteTempPDF()
        }
    }
}
