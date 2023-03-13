//
//  CatalystPluginImplementation.swift
//  SaneScanner-CatalystPlugin
//
//  Created by Stanislas Chevallier on 03/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import AppKit

@objc public class CatalystPluginImplementation: NSObject, CatalystPlugin {
    required public override init() {
    }
    
    public func presentHostInputAlert(title: String, message: String, hostPlaceholder: String, namePlaceholder: String, suggestedHost: String, suggestedName: String, add: String, cancel: String, completion: (String, String) -> ()) {
        let alert = NSAlert()

        let addButton = alert.addButton(withTitle: add)
        let cancelButton = alert.addButton(withTitle: cancel)
        alert.messageText = title
        alert.informativeText = message

        let fields = alert.setupTextFields([.regular, .regular], nextView: addButton, previousView: cancelButton)
        let hostField = fields[0]
        let nameField = fields[1]

        hostField.placeholderString = hostPlaceholder
        hostField.stringValue = suggestedHost

        nameField.placeholderString = namePlaceholder
        nameField.stringValue = suggestedName
        
        /*
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
            textFieldHost.nextKeyView = textFieldName
            textFieldName.nextKeyView = addButton
            cancelButton.nextKeyView = textFieldHost
        }*/

        if alert.runModal() == NSApplication.ModalResponse.alertFirstButtonReturn {
            completion(hostField.stringValue, nameField.stringValue)
        }
    }
    
    public func presentAuthInputAlert(title: String, message: String, usernamePlaceholder: String, passwordPlaceholder: String, continue: String, remember: String, cancel: String, completion: (_ username: String?, _ password: String?, _ remember: Bool) -> ()) {

        let alert = NSAlert()

        let continueButton = alert.addButton(withTitle: `continue`)
        alert.addButton(withTitle: remember)
        let cancelButton = alert.addButton(withTitle: cancel)
        alert.messageText = title
        alert.informativeText = message

        let fields = alert.setupTextFields([.regular, .secure], nextView: continueButton, previousView: cancelButton)
        let usernameField = fields[0]
        let passwordField = fields[1]

        usernameField.placeholderString = usernamePlaceholder
        passwordField.placeholderString = passwordPlaceholder

        let rv = alert.runModal()
        let cancelled = rv == NSApplication.ModalResponse.alertThirdButtonReturn
        completion(cancelled ? nil : usernameField.stringValue, cancelled ? nil : passwordField.stringValue, rv == NSApplication.ModalResponse.alertSecondButtonReturn)
    }
}

internal extension NSAlert {
    enum FieldKind {
        case regular
        case secure
    }
    func setupTextFields(_ kinds: [FieldKind], nextView: NSView, previousView: NSView) -> [NSTextField] {
        let fieldSize = NSSize(width: 230, height: 25)
        let margin: CGFloat = 5

        let fields = kinds.reversed().enumerated().map { (index: Int, kind: FieldKind) -> NSTextField in
            let frame = NSRect(
                x: 0, y: (fieldSize.height + margin) * CGFloat(index),
                width: fieldSize.width, height: fieldSize.height
            )
            let field: NSTextField
            switch kind {
            case .regular: field = NSTextField(frame: frame)
            case .secure:  field = NSSecureTextField(frame: frame)
            }
            
            field.stringValue = ""
            field.isBezeled = true
            field.bezelStyle = .roundedBezel
            return field
        }.reversed()

        let accessory = NSView(frame: NSRect(
            x: 0, y: 0,
            width: fieldSize.width, height: (fieldSize.height + margin) * CGFloat(kinds.count) - margin
        ))
        accessory.autoresizingMask = .width
        accessory.wantsLayer = true
        fields.forEach { accessory.addSubview($0) }
        accessory.translatesAutoresizingMaskIntoConstraints = false
        accessory.setContentHuggingPriority(.required, for: .vertical)
        accessoryView = accessory
        window.initialFirstResponder = fields.first
        
        let allViews: [NSView] = [previousView] + Array(fields) + [nextView]
        (0..<allViews.count - 1).forEach { index in
            allViews[index].nextKeyView = allViews[index + 1]
        }
        
        return Array(fields)
    }
}
