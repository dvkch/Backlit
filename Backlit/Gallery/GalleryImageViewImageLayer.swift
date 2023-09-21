//
//  GalleryImageViewImageLayer.swift
//  Backlit
//
//  Created by syan on 03/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

#if !targetEnvironment(macCatalyst)
class GalleryImageViewImageLayer: CALayer {
    var image: CGImage? {
        didSet {
            contents = image
        }
    }
}
#endif
