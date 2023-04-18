//
//  PDFActivity.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

#if !targetEnvironment(macCatalyst)
import UIKit
import SYKit

class PDFActivity: UIActivity {
    
    // MARK: Properties
    weak var presentingViewController: UIViewController?
    private weak var hud: HUDAlertController?
    
    // MARK: Original activity properties
    private var items: [URL] = []
    private weak var sourceView: UIView?
    private weak var barButtonItem: UIBarButtonItem?
    private var sourceRect = CGRect.zero
    private var permittedArrowDirections = UIPopoverArrowDirection.any

    // MARK: Methods
    func configure(using originalActivityViewController: UIActivityViewController) {
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
            .filter { ["png", "heic", "jpg"].contains($0.pathExtension.lowercased()) }
            .count
        
        return activityItems.count == readableCount
    }
    
    override func prepare(withActivityItems activityItems: [Any]) {
        super.prepare(withActivityItems: activityItems)
        self.items = activityItems
            .compactMap { $0 as? URL }
            .sorted { $0.lastPathComponent > $1.lastPathComponent }
        
        if #available(iOS 13.0, *) {
            // apparently after doing that on iOS 12 perform never gets called (something most be dealloced somewhere...)
            obtainPresentingViewController {
                self.hud = HUDAlertController.show(in: $0, animated: false)
            }
        }
    }
    
    private func obtainPresentingViewController(_ completion: @escaping (UIViewController) -> ()) {
        guard let presentingViewController = presentingViewController else { return }
        if let vc = presentingViewController.presentedViewController {
            vc.dismiss(animated: true, completion: {
                completion(presentingViewController)
            })
        }
        else {
            completion(presentingViewController)
        }
    }
    
    override func perform() {
        super.perform()
        let tempURL = GalleryManager.shared.tempPdfFileUrl()
        
        do {
            try PDFGenerator.generatePDF(destination: tempURL, images: self.items, pageSize: Preferences.shared.pdfSize)
        }
        catch {
            obtainPresentingViewController {
                UIAlertController.show(for: error, in: $0)
            }
            activityDidFinish(false)
            return
        }

        let vc = UIActivityViewController(activityItems: [tempURL], applicationActivities: nil)
        vc.popoverPresentationController?.barButtonItem = barButtonItem
        vc.popoverPresentationController?.sourceView = sourceView
        vc.popoverPresentationController?.sourceRect = sourceRect
        vc.popoverPresentationController?.permittedArrowDirections = permittedArrowDirections
        
        vc.completionWithItemsHandler = { activityType, completed, returnedItems, error in
            // is called when the interaction with the PDF is done. It's either been copied, imported,
            // displayed, shared or printed, but we can dispose of it
            GalleryManager.shared.deleteTempPDF()
            self.activityDidFinish(completed)
        }
        
        obtainPresentingViewController {
            $0.present(vc, animated: true, completion: nil)
        }
    }
}
#endif
