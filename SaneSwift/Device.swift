//
//  Device.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

// TODO: Move
public enum SaneStandardOption: CaseIterable {
    case preview, resolution, resolutionX, resolutionY, colorMode, areaTopLeftX, areaTopLeftY, areaBottomRightX, areaBottomRightY
    
    var saneIdentifier: String {
        switch self {
        case .preview:          return SANE_NAME_PREVIEW
        case .resolution:       return SANE_NAME_SCAN_RESOLUTION
        case .resolutionX:      return SANE_NAME_SCAN_X_RESOLUTION
        case .resolutionY:      return SANE_NAME_SCAN_Y_RESOLUTION
        case .colorMode:        return SANE_NAME_SCAN_MODE
        case .areaTopLeftX:     return SANE_NAME_SCAN_TL_X
        case .areaTopLeftY:     return SANE_NAME_SCAN_TL_Y
        case .areaBottomRightX: return SANE_NAME_SCAN_BR_X
        case .areaBottomRightY: return SANE_NAME_SCAN_BR_Y
        }
    }

    static var cropOptions: [SaneStandardOption] {
        return [.areaTopLeftX, .areaTopLeftY, .areaBottomRightX, .areaBottomRightY]
    }
}

@objc public enum SaneStandardOptionPreviewValue: Int {
    case auto, min, max
}

@objc public class SaneStandardOptionPreviewHelper: NSObject {
    @objc public class func forIdentifier(_ value: String) -> SaneStandardOptionPreviewValue {
        return SaneStandardOption.allCases.first(where: { $0.saneIdentifier == value })?.bestPreviewValue ?? .auto
    }
}

public extension SaneStandardOption {
    var bestPreviewValue: SaneStandardOptionPreviewValue {
        let meh = SANE_NAME_PREVIEW
        switch self {
        case .preview, .colorMode:                      return .auto
        case .resolution, .resolutionX, .resolutionY:   return .min
        case .areaTopLeftX, .areaTopLeftY:              return .min
        case .areaBottomRightX, .areaBottomRightY:      return .max
        }
    }
}

@objc public class Device: NSObject {
    // MARK: Initializers
    init(name: String, type: String, vendor: String, model: String) {
        self.name = name
        self.type = type
        self.vendor = vendor
        self.model = model
        super.init()
    }
    
    init(cDevice: SANE_Device) {
        self.name   = NSStringFromSaneString(cDevice.name)   ?? ""
        self.model  = NSStringFromSaneString(cDevice.model)  ?? ""
        self.vendor = NSStringFromSaneString(cDevice.vendor) ?? ""
        self.type   = Sane.shared.translation(for: NSStringFromSaneString(cDevice.type) ?? "")
        super.init()
    }
    
    // MARK: Device properties
    public let name: String
    public let type: String
    public let vendor: String
    public let model: String
    
    // MARK: Options
    public internal(set) var options = [SYSaneOption]() {
        didSet {
            updateCropArea()
        }
    }
    
    // MARK: Cache properties
    public var lastPreviewImage: UIImage?
    public var cropArea: CGRect = .zero
}

// MARK: Options
extension Device {
    public func groupedOptions(includeAdvanced: Bool) -> [SYSaneOptionGroup] {
        var filteredOptions = options
            .filter { $0.identifier != SaneStandardOption.preview.saneIdentifier }
        
        if canCrop {
            let cropOptionsIDs = SaneStandardOption.cropOptions.map { $0.saneIdentifier }
            filteredOptions.removeAll(where: { cropOptionsIDs.contains($0.identifier) })
        }
        
        if !includeAdvanced {
            filteredOptions.removeAll(where: { $0.capAdvanced })
        }
        
        return SYSaneOption.groupedElements(filteredOptions, removeEmptyGroups: true) ?? []
    }
    
    public func standardOption(for stdOption: SaneStandardOption) -> SYSaneOption? {
        return option(with: stdOption.saneIdentifier)
    }
    
    public func option(with identifier: String) -> SYSaneOption? {
        return options.first(where: { $0.identifier == identifier })
    }
}

// MARK: Derived capacities
extension Device {
    public var canCrop: Bool {
        let existingSettableOptions = SaneStandardOption.cropOptions
            .compactMap { standardOption(for: $0) }
            .filter { $0.capSettableViaSoftware && !$0.capInactive }
        return existingSettableOptions.count == 4
    }
    
    public var maxCropArea: CGRect {
        // TODO: clean up
        let options = SaneStandardOption.cropOptions
            .compactMap { standardOption(for: $0) as? SYSaneOptionNumber }
            .filter { $0.capSettableViaSoftware && !$0.capInactive }
        
        guard options.count == 4 else { return .zero }
        
        var tlX = Double(0), tlY = Double(0), brX = Double(0), brY = Double(0)
        
        options.forEach { (option) in
            if option.identifier == SaneStandardOption.areaTopLeftX.saneIdentifier {
                tlX = option.bestValueForPreview()?.doubleValue ?? 0
            }
            if option.identifier == SaneStandardOption.areaTopLeftY.saneIdentifier {
                tlY = option.bestValueForPreview()?.doubleValue ?? 0
            }
            if option.identifier == SaneStandardOption.areaBottomRightX.saneIdentifier {
                brX = option.bestValueForPreview()?.doubleValue ?? 0
            }
            if option.identifier == SaneStandardOption.areaBottomRightY.saneIdentifier {
                brY = option.bestValueForPreview()?.doubleValue ?? 0
            }
        }
        
        return CGRect(x: tlX, y: tlY, width: brX - tlX, height: brY - tlY)
    }
    
    public var previewImageRatio: CGFloat? {
        // TODO: was cached, still needed?
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
        // TODO: ugly, hacky, definitely wrong. fix when we get the chance, remove if unused
        return name.components(separatedBy: ":").first ?? ""
    }
}

