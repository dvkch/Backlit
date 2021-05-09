//
//  CatalystView.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 06/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import Foundation

@objc(CatalystView) public protocol CatalystView: NSObjectProtocol {
}

#if os(macOS)
import AppKit

extension NSView: CatalystView {}

extension CatalystView {
    var view: NSView {
        return self as! NSView
    }

    static var catalystScaling: CGFloat {
        if #available(macOS 11.0, *) {
            return 1 / 0.77
        }
        return 1
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

        containedView.scaleUnitSquare(to: NSSize(width: NSView.catalystScaling, height: NSView.catalystScaling))
    }

    override func layout() {
        NSAnimationContext.current.duration = 0
        super.layout()
    }

    override var intrinsicContentSize: NSSize {
        var size = containedView.intrinsicContentSize
        if size.width != NSView.noIntrinsicMetric {
            size.width *= NSView.catalystScaling
        }
        if size.height != NSView.noIntrinsicMetric {
            size.height *= NSView.catalystScaling
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

extension CatalystView {
    var view: UIView? {
        guard let containerClass = NSClassFromString("_UINSView") as? NSObject.Type else {
            return nil
        }
        
        let container = containerClass
            .perform(NSSelectorFromString("alloc")).takeUnretainedValue()
            .perform(NSSelectorFromString("initWithContentNSView:"), with: self).takeUnretainedValue()

        return container as? UIView
    }

    static var catalystScaling: CGFloat {
        if #available(macCatalyst 14.0, *) {
            return 1 / 0.77
        }
        else {
            return 1
        }
    }
}
#endif
