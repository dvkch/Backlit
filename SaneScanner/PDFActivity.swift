//
//  PDFActivity.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SVProgressHUD
import SYKit

@objc class PDFActivity: UIActivity {
    
    // MARK: Properties
    @objc weak var presentingViewController: UIViewController?
    
    // MARK: Original activity properties
    private var items: [URL] = []
    private weak var sourceView: UIView?
    private weak var barButtonItem: UIBarButtonItem?
    private var sourceRect = CGRect.zero
    private var permittedArrowDirections = UIPopoverArrowDirection.any

    // MARK: Methods
    @objc func configure(using originalActivityViewController: UIActivityViewController) {
        guard let popoverController = originalActivityViewController.popoverPresentationController else { return }
        barButtonItem   = popoverController.barButtonItem
        sourceView      = popoverController.sourceView
        sourceRect      = popoverController.sourceRect
        permittedArrowDirections = popoverController.permittedArrowDirections
    }

    // MARK: UIActivity
    override var activityType: UIActivity.ActivityType? {
        let id = Bundle.main.bundleIdentifier!.appending(".pdfActivity")
        return UIActivity.ActivityType(id)
    }
    
    override var activityTitle: String? {
        return "SHARE AS PDF".localized
    }
    
    override var activityImage: UIImage? {
        return UIImage(named: "make_pdf_activity")
    }
    
    override class var activityCategory: UIActivity.Category {
        return .action
    }
    
    override func canPerform(withActivityItems activityItems: [Any]) -> Bool {
        let readableCount = activityItems
            .compactMap { $0 as? URL }
            .filter { (url) -> Bool in
                guard let data = try? Data(contentsOf: url, options: .mappedIfSafe) else { return false }
                return (data as NSData).sy_imageDataIsValidPNG() || (data as NSData).sy_imageDataIsValidJPEG()
        }.count
        
        return activityItems.count == readableCount
    }
    
    override func prepare(withActivityItems activityItems: [Any]) {
        super.prepare(withActivityItems: activityItems)
        self.items = activityItems
            .compactMap { $0 as? URL }
            .sorted { $0.lastPathComponent > $1.lastPathComponent }
        SVProgressHUD.show()
    }
    
    override func perform() {
        super.perform()
        
        let tempPath = SYGalleryManager.shared.tempPDFPath()!
        
        do {
            try PDFGenerator.generatePDF(destination: URL(fileURLWithPath: tempPath), images: self.items, aspectRatio: 210 / 297, jpegQuality: 0.9, fixedPageSize: true)
        }
        catch {
            SVProgressHUD.showError(withStatus: error.localizedDescription)
            activityDidFinish(false)
            return
        }
        
        SVProgressHUD.dismiss()
        
        let vc = UIActivityViewController(activityItems: [URL(fileURLWithPath: tempPath)], applicationActivities: nil)
        vc.popoverPresentationController?.barButtonItem = barButtonItem
        vc.popoverPresentationController?.sourceView = sourceView
        vc.popoverPresentationController?.sourceRect = sourceRect
        vc.popoverPresentationController?.permittedArrowDirections = permittedArrowDirections
        
        vc.completionWithItemsHandler = { activityType, completed, returnedItems, error in
            // is called when the interaction with the PDF is done. It's either been copied, imported,
            // displayed, shared or printed, but we can dispose of it
            SYGalleryManager.shared.deleteTempPDFs()
            self.activityDidFinish(completed)
        }
        
        presentingViewController?.present(vc, animated: true, completion: nil)
    }
}

