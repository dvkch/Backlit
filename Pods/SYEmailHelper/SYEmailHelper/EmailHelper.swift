//
//  EmailHelper.swift
//  SYEmailHelper
//
//  Created by Stanislas Chevallier on 20/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

@objc public enum EmailHelperError: Int, Error, CustomNSError, LocalizedError {
    case noServiceAvailable
    case couldnotOpenThirdPartyApp
    
    var localizedDescription: String {
        switch self {
        case .noServiceAvailable:           return "No email service available"
        case .couldnotOpenThirdPartyApp:    return "Couldn't open third party email app"
        }
    }
}

@objcMembers public class EmailHelper : NSObject {
 
    public static let shared = EmailHelper()

    public var actionSheetTitle: String?
    public var actionSheetMessage: String?
    public var actionSheetCancelButtonText: String? = "Cancel"
    public var showCopyToPasteBoard: Bool = true
    
    public static var allThirdPartyServices: [ThirdPartyAppEmailService] {
        return [ThirdPartyAppEmailService.gmailService,
                ThirdPartyAppEmailService.googleInboxService,
                ThirdPartyAppEmailService.microsoftOutlookService,
                ThirdPartyAppEmailService.sparkService]
    }
    
    public static var allServices: [EmailService] {
        #if targetEnvironment(macCatalyst)
        return [MailtoLinkEmailService()]
        #else
        return allThirdPartyServices + [NativeEmailService(), PasteboardEmailService()]
        #endif
    }
    
    public static var availableServices: [EmailService] {
        return allServices.filter { $0.isAvailable }
    }
    
    public func presentActionSheet(
        address: String?,
        subject: String?,
        body: String?,
        presentingViewController: UIViewController,
        sender: AnyObject?,
        completion: ((_ cancelled: Bool, _ service: EmailService?, _ error: Error?) -> ())?)
    {
        var services = EmailHelper.availableServices
        
        if !showCopyToPasteBoard {
            services = services.filter { !$0.isKind(of: PasteboardEmailService.self) }
        }
        
        guard !services.isEmpty else {
            completion?(false, nil, EmailHelperError.noServiceAvailable)
            return
        }
        
        guard services.count > 1 else {
            services.first?.launch(
                address: address?.trimmingCharacters(in: .whitespacesAndNewlines),
                subject: subject?.trimmingCharacters(in: .whitespacesAndNewlines),
                body: body?.trimmingCharacters(in: .whitespacesAndNewlines),
                presentingViewController: presentingViewController) { (canceled, error) in
                    completion?(canceled, services.first, error)
            }
            return
        }
        
        let actionSheet = UIAlertController(title: actionSheetTitle, message: actionSheetMessage, preferredStyle: .actionSheet)
        
        for service in services {
            actionSheet.addAction(UIAlertAction(title: service.name, style: .default) { (_) in
                service.launch(
                    address: address?.trimmingCharacters(in: .whitespacesAndNewlines),
                    subject: subject?.trimmingCharacters(in: .whitespacesAndNewlines),
                    body: body?.trimmingCharacters(in: .whitespacesAndNewlines),
                    presentingViewController: presentingViewController) { (canceled, error) in
                        completion?(canceled, service, error)
                }
            })
        }
        
        actionSheet.addAction(UIAlertAction(title: actionSheetCancelButtonText, style: .cancel, handler: { (_) in
            completion?(true, nil, nil)
        }))
        
        if let sender = sender as? UIBarButtonItem {
            actionSheet.popoverPresentationController?.barButtonItem = sender
        }
        else if let sender = sender as? UIView {
            actionSheet.popoverPresentationController?.permittedArrowDirections = .any
            actionSheet.popoverPresentationController?.sourceView = presentingViewController.view
            actionSheet.popoverPresentationController?.sourceRect = presentingViewController.view.convert(sender.bounds, from: sender)
        }
        
        presentingViewController.present(actionSheet, animated: true, completion: nil)
    }
}
