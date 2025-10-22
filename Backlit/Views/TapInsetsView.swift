//
//  TapInsetsView.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

class TapInsetsView: UIView {
    
    // MARK: Init
    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    private func setup() {
        addInteraction(UIPointerInteraction(delegate: self))
     }
   
    // MARK: Tap insets
    var tapInsets: UIEdgeInsets = .zero
    
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        if tapInsets != .zero {
            if isHidden || alpha == 0 {
                return false
            }
            return bounds.inset(by: tapInsets).contains(point)
        }
        return super.point(inside: point, with: event)
    }
}

extension TapInsetsView: UIPointerInteractionDelegate {
    func pointerInteraction(_ interaction: UIPointerInteraction, styleFor region: UIPointerRegion) -> UIPointerStyle? {
        if let interactionView = interaction.view {
            let targetedPreview = UITargetedPreview(view: interactionView)
            return .init(effect: .automatic(targetedPreview))
        }
        return nil
    }
}
