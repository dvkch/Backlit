//
//  AVAsset+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 09/01/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import AVFoundation

public extension AVAsset {

    enum ThumbnailPosition: Double {
        case start  = 0.0
        case middle = 0.5
        case end    = 1.0
    }
    
    func getThumbnail(at position: ThumbnailPosition, closure: @escaping (UIImage?, Error?) -> Void) {
        self.getThumbnails(at: [position.rawValue]) { (images, error) in
            closure(images?.first, error)
        }
    }
    
    func getThumbnails(count: Int, closure: @escaping ([UIImage]?, Error?) -> Void) {
        guard count > 0 else {
            closure([], nil)
            return
        }
        
        guard count > 1 else {
            self.getThumbnails(at: [0], closure: closure)
            return
        }

        // evenly spaced from 0 to 1 included
        let step = Double(1) / Double(count - 1)
        let percents = (0..<count).map { Double($0) * step }
        getThumbnails(at: percents, closure: closure)
    }
    
    func getThumbnails(at percents: [Double], closure: @escaping ([UIImage]?, Error?) -> Void) {
        guard !percents.isEmpty else {
            closure([], nil)
            return
        }
        
        guard !Thread.current.isMainThread else {
            DispatchQueue.global(qos: .userInteractive).async {
                self.getThumbnails(at: percents, closure: closure)
            }
            return
        }
        
        let generator = AVAssetImageGenerator(asset: self)
        do {
            var thumbs = [UIImage]()
            for percent in percents {
                let time = CMTime(seconds: self.duration.seconds * percent, preferredTimescale: self.duration.timescale)
                let cgThumb = try generator.copyCGImage(at: time, actualTime: nil)
                thumbs.append(UIImage(cgImage: cgThumb))
            }
            DispatchQueue.main.async {
                closure(thumbs, nil)
            }
        } catch let error {
            DispatchQueue.main.async {
                closure(nil, error)
            }
        }
    }
}
