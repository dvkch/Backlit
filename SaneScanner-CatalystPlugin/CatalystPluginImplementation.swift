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

    public func presentHostInputAlert(title: String, message: String, add: String, cancel: String, completion: (String) -> ()) {
        let alert = NSAlert()

        alert.addButton(withTitle: add)
        alert.addButton(withTitle: cancel)
        alert.messageText = title
        alert.informativeText = message

        let textField = NSTextField(frame: NSRect(x: 0, y: 0, width: 200, height: 24))
        textField.stringValue = ""
        textField.isBezeled = true
        textField.bezelStyle = .roundedBezel
        alert.accessoryView = textField
        alert.window.initialFirstResponder = alert.accessoryView

        if alert.runModal() == NSApplication.ModalResponse.alertFirstButtonReturn {
            completion(textField.stringValue)
        }
    }

    public func dropdown(options: [CatalystDropdownValueProtocol], selectedIndex: Int, disabled: Bool, changed: @escaping (CatalystDropdownValueProtocol) -> ()) -> CatalystView {
        let view = NSPopUpButton()

        if selectedIndex >= 0 && selectedIndex < options.count {
            view.addItem(withTitle: options[selectedIndex].title)
            view.selectItem(at: 0)
        }
        view.isEnabled = !disabled
        
        NotificationCenter.default.addObserver(forName: NSPopUpButton.willPopUpNotification, object: view, queue: .main) { _ in
            view.removeAllItems()
            view.addItems(withTitles: options.map(\.title))
            view.selectItem(at: selectedIndex >= 0 ? selectedIndex : -1)
        }
        NotificationCenter.default.addObserver(forName: NSMenu.didSendActionNotification, object: view.menu, queue: .main) { _ in
            changed(options[view.indexOfSelectedItem])
        }
        return CatalystViewContainer(containing: view)
    }
}

