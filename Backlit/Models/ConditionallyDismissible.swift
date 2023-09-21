//
//  ConditionallyDismissible.swift
//  Backlit
//
//  Created by syan on 12/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

struct DismissalTexts {
    let title: String
    let message: String
    let dismissButton: String
    let cancelButton: String
}

protocol ConditionallyDismissible: UIViewController {
    var isDismissible: Bool { get }
    var dismissalConfirmationTexts: DismissalTexts { get }
}

extension ConditionallyDismissible {
    func showDismissalConfirmation(dismiss: @escaping () -> ()) {
        let texts = dismissalConfirmationTexts
        let alert = UIAlertController(
            title: texts.title,
            message: texts.message,
            preferredStyle: .alert
        )
        alert.addAction(title: texts.dismissButton, style: .destructive) { _ in
            dismiss()
        }
        alert.addAction(title: texts.cancelButton, style: .cancel, handler: nil)
        present(alert, animated: true)
    }
}
