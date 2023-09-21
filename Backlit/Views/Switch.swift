//
//  Switch.swift
//  Backlit
//
//  Created by syan on 21/09/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

class Switch: UISwitch {
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
        if #available(iOS 13.4, *) {
            addInteraction(UIPointerInteraction(delegate: self))
        }
    }
}

@available(iOS 13.4, *)
extension Switch: UIPointerInteractionDelegate {
    func pointerInteraction(_ interaction: UIPointerInteraction, styleFor region: UIPointerRegion) -> UIPointerStyle? {
        if let interactionView = interaction.view {
            let targetedPreview = UITargetedPreview(view: interactionView)
            return .init(effect: .lift(targetedPreview))
        }
        return nil
    }
}

