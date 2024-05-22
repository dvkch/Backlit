//
//  SaneMockable.swift
//  Backlit
//
//  Created by syan on 06/05/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SaneSwiftC
import SYKit
import AVFoundation

protocol SaneController {
    func scanSatus(for device: Device) -> Device.Status
    func previewImage(for device: Device) -> UIImage?
    func preview(device: Device, progress: ((Device.Status) -> ())?, completion: @escaping SaneCompletion<ScanImage>)
    func scan(device: Device, progress: ((Device.Status) -> ())?, completion: @escaping SaneCompletion<[ScanImage]>)
    func cancelCurrentScan()
}

extension Sane: SaneController {
    func previewImage(for device: Device) -> UIImage? {
        return device.lastPreviewImage
    }
}

#if !DEBUG
typealias SaneMockable = Sane
#endif

#if DEBUG
class SaneMockable {
    
    // MARK: Init
    static let shared = SaneMockable()
    
    private init() {
    }
    
    // MARK: Properties
    var isMockingEnabled: Bool = false
    
    var mockedScanImage: UIImage? = nil
    
    // MARK: Internal properties
    private let totalScanTime: TimeInterval = 4
    private let scanSteps: Int = 100
    private var stopScanOperation = false
    private var deviceStatuses = [Device: Device.Status]()
    private(set) var previewImages = [Device: UIImage]()
}

// MARK: SaneController
extension SaneMockable: SaneController {
    // MARK: Device status
    func previewImage(for device: Device) -> UIImage? {
        guard isMockingEnabled else {
            return Sane.shared.previewImage(for: device)
        }
        return previewImages[device] ?? nil
    }

    func scanSatus(for device: Device) -> Device.Status {
        guard isMockingEnabled else {
            return Sane.shared.scanSatus(for: device)
        }
        
        return deviceStatuses[device] ?? nil
    }
    
    // MARK: Scanning
    func preview(device: Device, progress: ((Device.Status) -> ())?, completion: @escaping SaneCompletion<ScanImage>) {
        guard isMockingEnabled else {
            return Sane.shared.preview(device: device, progress: progress, completion: completion)
        }
        
        internalScan(
            device: device, operation: .preview,
            cropArea: device.maxCropArea, generateIntermediateImages: true,
            progress: progress, completion: { result in
                completion(result.map { $0.last! })
            }
        )
    }
    
    func scan(device: Device, progress: ((Device.Status) -> ())?, completion: @escaping SaneCompletion<[ScanImage]>) {
        guard isMockingEnabled else {
            return Sane.shared.scan(device: device, progress: progress, completion: completion)
        }

        internalScan(
            device: device, operation: .scan,
            cropArea: device.cropArea, generateIntermediateImages: false,
            progress: progress, completion: completion
        )
    }
    
    func cancelCurrentScan() {
        guard isMockingEnabled else {
            return Sane.shared.cancelCurrentScan()
        }

        self.stopScanOperation = true
    }
    
    // MARK: Internal
    private func mockedScanImage(for device: Device, cropArea: CGRect) -> CGImage? {
        let mockedScanImage = self.mockedScanImage ?? UIImage(color: .tint, size: device.maxCropArea.size)!
        
        let fullSizedRect = AVMakeRect(
            aspectRatio: device.maxCropArea.size,
            insideRect: CGRect(origin: .zero, size: mockedScanImage.size)
        )
        guard let imageCroppedInScanMaxArea = mockedScanImage.cgImage?.cropping(to: fullSizedRect) else { return nil }

        let cropArea = cropArea
            .asPercents(of: device.maxCropArea)
            .fromPercents(of: CGRect(x: 0, y: 0, width: imageCroppedInScanMaxArea.width, height: imageCroppedInScanMaxArea.height))

        let imageCroppedInCurrentScanArea = imageCroppedInScanMaxArea.cropping(to: cropArea)

        let isRGB = device.standardOption(for: .colorMode)?.localizedValue == "Color".saneTranslation
        return isRGB ? imageCroppedInCurrentScanArea : imageCroppedInCurrentScanArea?.greyscale(using: .noir)
    }
    
    private func mockedScanParameters(for device: Device, cropArea: CGRect, image: CGImage) -> ScanParameters {
        let isRGB = device.standardOption(for: .colorMode)?.localizedValue == "Color".saneTranslation
        
        return ScanParameters(
            currentlyAcquiredFrame: isRGB ? SANE_FRAME_RGB : SANE_FRAME_GRAY,
            acquiringLastFrame: true,
            bytesPerLine: image.bytesPerRow,
            width: image.width,
            height: image.height,
            depth: image.bitsPerComponent,
            cropArea: cropArea
        )
    }
    
    private func mockedIntermediateImage(from image: CGImage, progress: Float) -> UIImage? {
        return autoreleasepool {
            let visibleHeight = Int(Float(image.height) * progress)

            guard let context = CGContext(
                data: nil,
                width: image.width, height: image.height,
                bitsPerComponent: image.bitsPerComponent,
                bytesPerRow: image.bytesPerRow,
                space: image.colorSpace ?? CGColorSpaceCreateDeviceRGB(),
                bitmapInfo: image.bitmapInfo.rawValue
            ) else { return nil }

            context.draw(image, in: CGRect(x: 0, y: 0, width: image.width, height: image.height))
            context.setFillColor(UIColor.white.cgColor)
            context.fill([CGRect(x: 0, y: 0, width: image.width, height: image.height - visibleHeight)])
            guard let cgImage = context.makeImage() else { return nil }
            return UIImage(cgImage: cgImage)
        }
    }

    private func internalScan(
        device: Device, operation: ScanOperation,
        cropArea: CGRect, generateIntermediateImages: Bool,
        progress: ((Device.Status) -> ())?, completion: @escaping SaneCompletion<[ScanImage]>
    ) {
        guard let image = mockedScanImage(for: device, cropArea: cropArea) else {
            return completion(.failure(SaneError.cannotGenerateImage))
        }
        let parameters = mockedScanParameters(for: device, cropArea: cropArea, image: image)
        
        let reportProgress = { (p: ScanProgress) in
            DispatchQueue.main.async {
                self.deviceStatuses[device] = (operation, p)
                progress?((operation, p))
            }
        }

        DispatchQueue.global(qos: .userInteractive).async {
            reportProgress(.warmingUp)
            sleep(1)

            var currentScanStep = 0
            while currentScanStep < self.scanSteps {
                if self.stopScanOperation {
                    reportProgress(.cancelling)
                    break
                }

                currentScanStep += 1
                usleep(UInt32(self.totalScanTime * 1_000_000 / TimeInterval(self.scanSteps)))
                
                let p = Float(currentScanStep) / Float(self.scanSteps)
                if generateIntermediateImages {
                    let intermediateImage = self.mockedIntermediateImage(from: image, progress: p)
                    reportProgress(.scanning(progress: p, finishedDocs: 0, incompletePreview: intermediateImage, parameters: parameters))
                }
                else {
                    reportProgress(.scanning(progress: p, finishedDocs: 0, incompletePreview: nil, parameters: parameters))
                }
            }
            usleep(500_000)

            DispatchQueue.main.async {
                self.deviceStatuses[device] = nil
                
                if self.stopScanOperation {
                    self.stopScanOperation = false
                    completion(.failure(SaneError.cancelled))
                }
                else {
                    let finalImage = UIImage(cgImage: image)
                    if cropArea == device.maxCropArea {
                        self.previewImages[device] = finalImage
                    }
                    completion(.success([(finalImage, parameters)]))
                }
            }
        }
    }
}
#endif
