//
//  GalleryItem+UI.swift
//  Backlit
//
//  Created by syan on 26/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import SYKit

// MARK: Generated labels
extension GalleryItem {
    private static let dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateStyle = .long
        formatter.timeStyle = .short
        formatter.locale = .autoupdatingCurrent
        formatter.timeZone = .autoupdatingCurrent
        formatter.formattingContext = .beginningOfSentence
        formatter.doesRelativeDateFormatting = true
        return formatter
    }()
    
    func creationDateString(includingTime: Bool, allowRelative: Bool) -> String {
        type(of: self).dateFormatter.timeStyle = includingTime ? .short : .none
        type(of: self).dateFormatter.doesRelativeDateFormatting = allowRelative
        return type(of: self).dateFormatter.string(from: creationDate)
    }
    
    func suggestedDescription(separator: String) -> String? {
        return [
            creationDateString(includingTime: true, allowRelative: true),
            deviceInfoString
        ].removingNils().joined(separator: separator)
    }

    var suggestedAccessibilityLabel: String? {
        return [
            L10n.galleryItem,
            suggestedDescription(separator: "; ")
        ].removingNils().joined(separator: "; ")
    }
}

// MARK: Actions
#if !targetEnvironment(macCatalyst)
extension GalleryItem {
    func contextMenuConfiguration(for viewController: UIViewController, sender: UIView, openGallery: @escaping (() -> ())) -> UIContextMenuConfiguration {
        return UIContextMenuConfiguration(
            identifier: nil,
            previewProvider: {
                return GalleryImagePreviewVC(item: self)
            },
            actionProvider: { _ in
                var actions = [UIAction]()
                actions.append(UIAction(title: L10n.actionOpen, image: .icon(.open)) { _ in
                    openGallery()
                })
                actions.append(UIAction(title: L10n.actionShare, image: .icon(.share)) { _ in
                    UIActivityViewController.showForURLs([self.url], from: .view(sender), presentingVC: viewController)
                })
                actions.append(UIAction(title: L10n.actionSaveToPhotos, image: .icon(.save)) { _ in
                    let hud = HUDAlertController.show(in: viewController)
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
                        GalleryManager.shared.saveItemToCamraRoll(self, completion: { result in
                            HUDAlertController.dismiss(hud) {
                            if case .failure(let error) = result {
                                    UIAlertController.show(for: error, in: viewController)
                                }
                            }
                        })
                    }
                })
                actions.append(UIAction(title: L10n.actionCopy, image: .icon(.copy)) { _ in
                    guard let data = try? Data(contentsOf: self.url, options: .mappedIfSafe) else { return }
                    UIPasteboard.general.setValue(data, forPasteboardType: self.writableTypeIdentifiersForItemProvider.first!)
                })
                actions.append(UIAction(title: L10n.actionDelete, image: .icon(.delete), attributes: .destructive) { _ in
                    viewController.deleteGalleryItems([self], sender: sender)
                })
                return UIMenu(title: self.suggestedDescription(separator: "\n") ?? "", children: actions)
            }
        )
    }
}

// MARK: VC
extension UIViewController {
    func deleteGalleryItems(_ items: [GalleryItem], sender: NSObject, completion: (() -> ())? = nil) {
        let alert = UIAlertController(
            title: L10n.dialogDeleteScansTitle,
            message: L10n.DialogDeleteScansMessage.quantity(items.count),
            preferredStyle: .actionSheet
        )
        alert.addAction(UIAlertAction(title: L10n.actionDelete, style: .destructive, handler: { (_) in
            let hud = HUDAlertController.show(in: self)
            items.forEach { (item) in
                GalleryManager.shared.deleteItem(item)
            }
            hud.dismiss(animated: true, completion: nil)
            completion?()
        }))
        alert.addAction(UIAlertAction(title: L10n.actionCancel, style: .cancel, handler: nil))
        alert.popoverPresentationController?.barButtonItem = sender as? UIBarButtonItem
        alert.popoverPresentationController?.sourceView = sender as? UIView
        alert.popoverPresentationController?.sourceRect = (sender as? UIView)?.bounds ?? .zero
        present(alert, animated: true, completion: nil)
    }
}
#endif
