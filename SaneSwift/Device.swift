//
//  Device.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

public class Device {
    // MARK: Initializers
    init(name: String, type: String, vendor: String, model: String) {
        self.name = name
        self.type = type
        self.vendor = vendor
        self.model = model
    }
    
    init(cDevice: SANE_Device) {
        self.name   = cDevice.name.asString()   ?? ""
        self.model  = cDevice.model.asString()  ?? ""
        self.vendor = cDevice.vendor.asString() ?? ""
        self.type   = cDevice.type.asString()?.saneTranslation ?? ""
    }
    
    // MARK: Device properties
    public let name: String
    public let type: String
    public let vendor: String
    public let model: String
    
    // MARK: Options
    public internal(set) var options = [DeviceOption]() {
        didSet {
            updateCropArea()
        }
    }
    
    // MARK: Cache properties
    public var lastPreviewImage: UIImage?
    public var cropArea: CGRect = .zero
}

// MARK: Equatable
extension Device: Equatable {
    public static func == (lhs: Device, rhs: Device) -> Bool {
        return lhs.name == rhs.name
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
        
        guard name.contains(":") else {
            // according to man page, if the device name doesn't contain a ":", this is the default host, the last one in the configuration
            return Sane.shared.configuration.hosts.last ?? ""
        }
        
        // if the host name contains ":" (IPv6 address for instance), it will be between brackets
        if let closingBracketIndex = name.firstIndex(of: "]") {
            let firstPart = name.substring(to: closingBracketIndex)
            let secondPart = name.substring(from: closingBracketIndex).components(separatedBy: ":").first ?? ""
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



