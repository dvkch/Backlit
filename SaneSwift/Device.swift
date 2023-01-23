//
//  Device.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import UIKit

public class Device {

    // MARK: Initializers
    init(name: String, type: String, vendor: String, model: String) {
        self.name = Name(rawValue: name)
        self.type = type
        self.vendor = vendor
        self.model = model
    }
    
    init(cDevice: SANE_Device) {
        self.name   = Name(rawValue: cDevice.name.asString()   ?? "")
        self.model  = cDevice.model.asString()                 ?? ""
        self.vendor = cDevice.vendor.asString()                ?? ""
        self.type   = cDevice.type.asString()?.saneTranslation ?? ""
    }
    
    // MARK: Device properties
    public let name: Name
    public let type: String
    public let vendor: String
    public let model: String
    
    // MARK: Options
    public internal(set) var options = [DeviceOption]() {
        didSet {
            updateCropArea()
        }
    }
    
    public var optionsDescription: String {
        return options.map { option in
            if let group = option as? DeviceOptionGroup {
                return group.localizedTitle
            } else {
                return "- \(option.localizedTitle): \(option.localizedValue)"
            }
        }.joined(separator: "\n")
    }
    
    // MARK: SaneSwift properties
    public var cropArea: CGRect = .zero

    public internal(set) var currentOperation: (operation: ScanOperation, progress: ScanProgress)?

    public var isScanning: Bool {
        switch currentOperation?.1 {
        case .warmingUp, .scanning: return true
        case .none, .cancelling: return false
        }
    }
    
    // MARK: Cache properties
    public private(set) var lastPreviewImage: UIImage?

    internal func updatePreviewImage(_ image: UIImage?, scannedWith parameters: ScanParameters?, fallbackToExisting: Bool) {
        guard let image = image else {
            if !fallbackToExisting {
                lastPreviewImage = nil
            }
            return
        }

        // update only if we scanned without cropping
        if let crop = parameters?.cropArea, crop != maxCropArea { return }

        // prevent keeping a scan image if resolution is very high. A color A4 150dpi (6.7MB) is used as maximum
        if let parameters = parameters, parameters.fileSize > 8_000_000 { return }

        // if we require color mode to be set to auto, update only if auto is not available or scan mode is color
        if Sane.shared.configuration.previewWithAutoColorMode, let parameters = parameters, parameters.currentlyAcquiredChannel != SANE_FRAME_RGB { return }
        
        // all seems good, let's keep it
        lastPreviewImage = image
    }
}

// MARK: Equatable
extension Device: Equatable {
    public static func == (lhs: Device, rhs: Device) -> Bool {
        return lhs.name == rhs.name
    }
}

// MARK: Name
extension Device {
    public struct Name: CustomStringConvertible, Equatable, Hashable {
        public let rawValue: String
        internal init(rawValue: String) {
            self.rawValue = rawValue
        }
        
        public var description: String {
            return rawValue
        }
    }
}

// MARK: Options
extension Device {
    public func optionGroups(includeAdvanced: Bool) -> [DeviceOptionGroup] {
        let groups = options
            .compactMap { $0 as? DeviceOptionGroup }
            .filter { !$0.options(includeAdvanced: includeAdvanced).isEmpty }

        return groups
    }
    
    public func standardOption(for stdOption: SaneStandardOption) -> DeviceOption? {
        return option(with: stdOption.saneIdentifier)
    }
    
    public func option(with identifier: String) -> DeviceOption? {
        return options.first(where: { $0.identifier == identifier })
    }
}

// MARK: Derived capacities
extension Device {
    public var canCrop: Bool {
        let existingSettableOptions = SaneStandardOption.cropOptions
            .compactMap { standardOption(for: $0) }
            .filter { $0.capabilities.isSettable && $0.capabilities.isActive }
        return existingSettableOptions.count == 4
    }
    
    public var maxCropArea: CGRect {
        let options = SaneStandardOption.cropOptions
            .compactMap { standardOption(for: $0) as? (DeviceOption & DeviceOptionNumeric) }
            .filter { $0.capabilities.isSettable && $0.capabilities.isActive }
        
        guard options.count == 4 else { return .zero }
        
        var tlX = Double(0), tlY = Double(0), brX = Double(0), brY = Double(0)
        
        options.forEach { (option) in
            var value: Double = 0
            if case let .value(doubleValue) = option.bestPreviewDoubleValue {
                value = doubleValue
            }
            
            if option.identifier == SaneStandardOption.areaTopLeftX.saneIdentifier {
                tlX = value
            }
            if option.identifier == SaneStandardOption.areaTopLeftY.saneIdentifier {
                tlY = value
            }
            if option.identifier == SaneStandardOption.areaBottomRightX.saneIdentifier {
                brX = value
            }
            if option.identifier == SaneStandardOption.areaBottomRightY.saneIdentifier {
                brY = value
            }
        }

        return CGRect(x: tlX, y: tlY, width: brX - tlX, height: brY - tlY)
    }
    
    public var previewImageRatio: CGFloat? {
        guard !options.isEmpty else { return nil }
        if maxCropArea != .zero {
            return maxCropArea.width / maxCropArea.height
        }
        if let image = lastPreviewImage {
            return image.size.width / image.size.height
        }
        return nil
    }
    
    private func updateCropArea() {
        let maxCropArea = self.maxCropArea
        guard maxCropArea != .zero else {
            cropArea = .zero
            return
        }
        
        // keep old crop values if possible in case all options are refreshed
        cropArea = maxCropArea.intersection(cropArea)
        
        // define default value if current crop area is not defined
        if cropArea == .zero {
            cropArea = maxCropArea
        }
    }
}

// MARK: Helpers
extension Device {
    public var host: String {
        // documentation about net backend names is available at https://www.systutorials.com/docs/linux/man/5-sane-net/
        let name = self.name.rawValue

        guard name.contains(":") else {
            // according to man page, if the device name doesn't contain a ":", this is the default host, the last one in the configuration
            return Sane.shared.configuration.hosts.last ?? ""
        }
        
        // if the host name contains ":" (IPv6 address for instance), it will be between brackets
        if let closingBracketIndex = name.firstIndex(of: "]") {
            let firstPart = name[name.startIndex...closingBracketIndex]
            let secondPart = name[closingBracketIndex..<name.endIndex].components(separatedBy: ":").first ?? ""
            return firstPart + secondPart
        }
        
        return name.components(separatedBy: ":").first ?? ""
    }
}

// MARK: CustomStringConvertible
extension Device : CustomStringConvertible {
    public var description: String {
        return "Device: \(name), \(type), \(vendor), \(model), \(options.count) options"
    }
}



