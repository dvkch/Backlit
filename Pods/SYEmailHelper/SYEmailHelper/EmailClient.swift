//
//  EmailClient.swift
//  SYEmailHelper
//
//  Created by Stanislas Chevallier on 20/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import Foundation
import MessageUI

// MARK: Main protocol definition
@objc public protocol EmailService: NSObjectProtocol {
    @objc var name: String { get }
    
    @objc func launch(address: String?, subject: String?, body: String?, presentingViewController: UIViewController, completion: ((_ cancelled: Bool, _ error: Error?) -> ())?)
    
    @objc var isAvailable: Bool { get }
}

// MARK: Third party services, launched via their URL scheme
@objcMembers public class ThirdPartyAppEmailService: NSObject {
    public let name: String
    let urlScheme: String
    let addressQueryName: String?
    let subjectQueryName: String?
    let bodyQueryName: String?
    
    public init(name: String, urlScheme: String, addressQueryName: String?, subjectQueryName: String?, bodyQueryName: String?) {
        self.name = name
        self.urlScheme = urlScheme
        self.addressQueryName = addressQueryName
        self.subjectQueryName = subjectQueryName
        self.bodyQueryName = bodyQueryName
        super.init()
    }
    
    public static var gmailService: ThirdPartyAppEmailService {
        return ThirdPartyAppEmailService(name: "Gmail", urlScheme: "googlegmail://co", addressQueryName: "to", subjectQueryName: "subject", bodyQueryName: "body")
    }
    
    public static var googleInboxService: ThirdPartyAppEmailService {
        return ThirdPartyAppEmailService(name: "Google Inbox", urlScheme: "inbox-gmail://co", addressQueryName: "to", subjectQueryName: "subject", bodyQueryName: "body")
    }
    
    public static var microsoftOutlookService: ThirdPartyAppEmailService {
        return ThirdPartyAppEmailService(name: "Microsoft Outlook", urlScheme: "ms-outlook://compose", addressQueryName: "to", subjectQueryName: "subject", bodyQueryName: "body")
    }
    
    public static var sparkService: ThirdPartyAppEmailService {
        return ThirdPartyAppEmailService(name: "Spark", urlScheme: "readdle-spark://compose", addressQueryName: "recipient", subjectQueryName: "subject", bodyQueryName: "body")
    }
}

extension ThirdPartyAppEmailService : EmailService {
    public func launch(address: String?, subject: String?, body: String?, presentingViewController: UIViewController, completion: ((Bool, Error?) -> ())?) {
        guard var components = URLComponents(string: urlScheme) else {
            completion?(false, NSError(domain: NSURLErrorDomain, code: NSURLErrorResourceUnavailable, userInfo: nil))
            return
        }
        
        var queryItems = [URLQueryItem]()
        if let addressQueryName = addressQueryName, let address = address {
            queryItems.append(URLQueryItem(name: addressQueryName, value: address))
        }
        if let subjectQueryName = subjectQueryName, let subject = subject {
            queryItems.append(URLQueryItem(name: subjectQueryName, value: subject))
        }
        if let bodyQueryName = bodyQueryName, let body = body {
            queryItems.append(URLQueryItem(name: bodyQueryName, value: body))
        }

        components.queryItems = queryItems
        
        guard let launchURL = components.url else {
            completion?(false, NSError(domain: NSURLErrorDomain, code: NSURLErrorResourceUnavailable, userInfo: nil))
            return
        }
        
        let result = UIApplication.shared.openURL(launchURL)
        
        
        if result {
            completion?(false, nil)
        } else {
            completion?(false, NSError(domain: NSURLErrorDomain, code: NSURLErrorResourceUnavailable, userInfo: nil))
        }
    }
    
    public var isAvailable: Bool {
        guard let url = URL(string: urlScheme) else { return false }
        return UIApplication.shared.canOpenURL(url)
    }
}

// MARK: In-App Native Apple Mail
@objcMembers public class NativeEmailService: NSObject, EmailService, MFMailComposeViewControllerDelegate {
    
    private var selfRetain: Any?
    private var completionBlock: ((_ cancelled: Bool, _ error: Error?) -> ())?

    public var name: String {
        return "Apple Mail"
    }
    
    public var isAvailable: Bool {
        return MFMailComposeViewController.canSendMail()
    }
    
    public func launch(address: String?, subject: String?, body: String?, presentingViewController: UIViewController, completion: ((Bool, Error?) -> ())?) {
        self.selfRetain = self
        self.completionBlock = completion
        
        let vc = MFMailComposeViewController()
        vc.setSubject(subject ?? "")
        if let address = address {
            vc.setToRecipients([address])
        }
        vc.setMessageBody(body ?? "", isHTML: false)
        vc.mailComposeDelegate = self

        presentingViewController.present(vc, animated: true, completion: nil)
    }
    
    public func mailComposeController(_ controller: MFMailComposeViewController, didFinishWith result: MFMailComposeResult, error: Error?) {
        controller.dismiss(animated: true, completion: nil)
        completionBlock?(result == .cancelled, error)
        completionBlock = nil
        selfRetain = nil
    }
}

// MARK: Last resort: copy to clipboard
@objcMembers public class PasteboardEmailService: NSObject {
    public static var name: String = "Copy address to pasteboard"
}

extension PasteboardEmailService : EmailService {
    public var name: String {
        return PasteboardEmailService.name
    }
    
    public var isAvailable: Bool {
        return true
    }
    
    public func launch(address: String?, subject: String?, body: String?, presentingViewController: UIViewController, completion: ((Bool, Error?) -> ())?) {
        UIPasteboard.general.string = address
        completion?(false, nil)
    }
}


