//
//  Device.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import UIKit

public class Device {

    // MARK: Initializers
    init(cDevice: SANE_Device) {
        self.name   = Name(rawValue: cDevice.name.asString()        ?? "")
        self.model  = cDevice.model.asString()?.saneSplit().first   ?? "" // epsonscan2 reports "ET-2810 Series:001:004", let's fix that
        self.vendor = cDevice.vendor.asString()                     ?? ""
        self.type   = cDevice.type.asString()?.saneTranslation      ?? ""
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
    internal var currentStatus: Status = nil

    // MARK: Cache properties
    public private(set) var lastPreviewImage: UIImage?

    internal func updatePreviewImage(_ scan: ScanImage?, afterOperation: ScanOperation, fallbackToExisting: Bool) {
        guard let scan else {
            if !fallbackToExisting {
                lastPreviewImage = nil
            }
            return
        }

        // update only if we scanned without cropping
        guard scan.parameters.cropArea == maxCropArea else { return }

        // prevent keeping a scan image if resolution is very high
        #if targetEnvironment(macCatalyst)
        // 1 page color A4 600dpi (~100MB) is used as maximum for macOS
        let maximumSize: Int = 110_000_000
        #else
        // 1 page color A4 300dpi (~25MB) is used as maximum for iOS
        let maximumSize: Int = 30_000_000
        #endif
        if afterOperation == .scan && scan.parameters.fileSize > maximumSize {
            return
        }

        // if we require color mode to be set to auto, update only if auto is not available or scan mode is color
        if standardOption(for: .preview) == nil,
           Sane.shared.configuration.previewWithAutoColorMode,
           scan.parameters.currentlyAcquiredFrame == SANE_FRAME_GRAY
        {
            return
        }
        
        // all seems good, let's keep it
        lastPreviewImage = scan.image
    }
}

// MARK: Equatable
extension Device: Hashable {
    public static func == (lhs: Device, rhs: Device) -> Bool {
        return lhs.name == rhs.name
    }
    public func hash(into hasher: inout Hasher) {
        hasher.combine(name.rawValue)
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

// MARK: Status
extension Device {
    public typealias Status = (operation: ScanOperation, progress: ScanProgress)?
}
extension Device.Status {
    public var isScanning: Bool {
        switch self?.1 {
        case .warmingUp, .scanning: return true
        case .none, .cancelling: return false
        }
    }
}

// MARK: Handle
extension Device {
    internal struct Handle {
        let pointer: SANE_Handle
        init?(pointer: SANE_Handle?) {
            guard let pointer else {
                return nil
            }
            self.pointer = pointer
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
    
    public var isUsingADFSource: Bool {
        // this is absolutely not garanteed by SANE, might only work on the test backend
        // https://gitlab.com/sane-project/backends/-/issues/662
        let currentSource = (standardOption(for: .source) as? DeviceOptionTyped<String>)?.value
        return currentSource == "Automatic Document Feeder"
    }
}

// MARK: Helpers
extension Device {
    public var host: SaneHost {
        // documentation about net backend names is available at https://www.systutorials.com/docs/linux/man/5-sane-net/
        let name = self.name.rawValue

        guard name.contains(":") else {
            // according to man page, if the device name doesn't contain a ":", this is the default host, the last one in the configuration
            return (Sane.shared.configuration.hosts + Sane.shared.configuration.transientdHosts).last!
        }
        
        guard let hostID = name.saneSplit().first(where: { $0 != "net" }) else {
            fatalError("Couldn't find host name")
        }
        return (Sane.shared.configuration.hosts.find(hostID) ??
                Sane.shared.configuration.transientdHosts.find(hostID) ??
                SaneHost(hostname: hostID, displayName: hostID))
    }
}

// MARK: CustomStringConvertible
extension Device : CustomStringConvertible {
    public var description: String {
        return "Device: \(name), \(type), \(vendor), \(model), \(options.count) options"
    }
}
