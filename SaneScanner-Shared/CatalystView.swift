//
//  CatalystView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation

@objc(CatalystView) public protocol CatalystView: NSObjectProtocol {
    var originalView: NSObject { get }
    func triggerAction(position: CGPoint)
}

#if os(macOS)
import AppKit

let catalystViewScaling: CGFloat = {
    if #available(macOS 11.0, *) {
        return 0.77
    }
    return 1
}()

class CatalystViewImplementation: NSObject, CatalystView {
    init(originalView: NSView) {
        self.originalView = originalView
        super.init()
    }

    let originalView: NSObject
    
    func triggerAction(position: CGPoint) {
        if let container = originalView as? CatalystViewContainer {
            CatalystViewImplementation(originalView: container.containedView).triggerAction(position: position)
            return
        }
        if let button = originalView as? NSPopUpButton {
            guard let window = NSApp.keyWindow?.contentView else { return }

            NotificationCenter.default.post(name: NSPopUpButton.willPopUpNotification, object: button)
            var nsPosition = NSPoint(x: position.x * catalystViewScaling, y: position.y * catalystViewScaling)
            nsPosition.y = window.bounds.height - nsPosition.y

            button.menu?.popUp(positioning: button.selectedItem, at: nsPosition, in: window)
            return
        }
        if let button = originalView as? NSButton {
            button.performClick(button)
            return
        }
    }
}

extension CatalystView {
    var containerWithProperScaling: CatalystView {
        return CatalystViewImplementation(originalView: CatalystViewContainer(containing: originalView as! NSView))
    }
}

internal class CatalystViewContainer: NSView {
    let containedView: NSView
    
    init(containing view: NSView) {
        self.containedView = view
        super.init(frame: .zero)
        
        containedView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(containedView)
        NSLayoutConstraint.activate([
            containedView.topAnchor.constraint(equalTo: topAnchor),
            containedView.leftAnchor.constraint(equalTo: leftAnchor),
            containedView.rightAnchor.constraint(equalTo: rightAnchor),
            containedView.bottomAnchor.constraint(equalTo: bottomAnchor),
        ])

        containedView.scaleUnitSquare(to: NSSize(width: 1 / catalystViewScaling, height: 1 / catalystViewScaling))
    }

    override func layout() {
        NSAnimationContext.current.duration = 0
        super.layout()
    }

    override var intrinsicContentSize: NSSize {
        var size = containedView.intrinsicContentSize
        if size.width != NSView.noIntrinsicMetric {
            size.width /= catalystViewScaling
        }
        if size.height != NSView.noIntrinsicMetric {
            size.height /= catalystViewScaling
        }
        return size
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}
#endif

#if os(iOS)
import UIKit

let catalystViewScaling: CGFloat = {
    if #available(macCatalyst 14.0, *) {
        return 0.77
    }
    else {
        return 1
    }
}()

extension CatalystView {
    private var containerClassName: String {
        // _UINSView
        return ["_UIN", String("weiVS".reversed())].joined()
    }
    
    private var containerInitSelector: String {
        // initWithContentNSView:
        return ["initWithCon", String(":weiVSNtnet".reversed())].joined()
    }
    
    private var containerContentSelector: String {
        // contentNSView
        return ["conten", String("weiVSNt".reversed())].joined()
    }
    
    var view: UIView? {
        if let originalView = originalView as? UIView {
            return originalView
        }

        guard let containerClass = NSClassFromString(containerClassName) as? NSObject.Type else {
            return nil
        }
        
        let container = containerClass
            .perform(NSSelectorFromString("alloc")).takeUnretainedValue()
            .perform(NSSelectorFromString(containerInitSelector), with: originalView).takeUnretainedValue()

        (container as? UIView)?.accessibilityIdentifier = String(describing: self)
        return (container as? UIView)
    }
    
    var nsView: CatalystView? {
        guard let object = self as? NSObject else { return nil }
        guard object.responds(to: NSSelectorFromString(containerContentSelector)) else { return nil }
        return object.value(forKey: containerContentSelector) as? CatalystView
    }
}


extension UIView: CatalystView {
    public var originalView: NSObject { self }
    public func triggerAction(position: CGPoint) {
    }
}
#endif
