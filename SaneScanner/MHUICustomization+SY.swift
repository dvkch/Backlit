//
//  MHUICustomization+SY.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 07/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import MHVideoPhotoGallery

extension MHUICustomization {
    static var sy_defaultTheme: MHUICustomization {
        let theme = MHUICustomization()
        theme.setMHGalleryBackgroundColor(.groupTableViewBackground, for: .imageViewerNavigationBarShown)
        theme.showMHShareViewInsteadOfActivityViewController = false
        theme.showOverView = true
        theme.useCustomBackButtonImageOnImageViewer = true
        theme.backButtonState = .withBackArrow
        theme.overviewTitle = "GALLERY OVERVIEW TITLE".localized
        theme.overviewCollectionViewCellClass = MediaPreviewCollectionViewCell.self
        
        let layout = GridLayout()
        layout.maxSize = 320/3
        layout.margin = 2
        theme.overviewCollectionViewLayout = layout
        
        MHGalleryCustomLocalizationBlock { (string) -> String? in
            let localized = string?.localized
            return string == localized ? nil : localized
        }
        
        return theme
    }
}

