//
//  Sane.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

@objc public protocol SaneDelegate: NSObjectProtocol {
    func saneDidStartUpdatingDevices(_ sane: Sane)
    func saneDidEndUpdatingDevices(_ sane: Sane)
    func saneNeedsAuth(_ sane: Sane, for device: String?, completion: @escaping (DeviceAuthentication?) -> ())
}

@objc public class Sane: NSObject {
    
    // MARK: Init
    @objc public static let shared = Sane()
    
    private override init() {
        super.init()
        
        thread = Thread(target: self, selector: #selector(self.threadKeepAlive), object: nil)
        thread.name = "SaneSwift"
        thread.start()
        
        #if DEBUG
        SaneSetLogLevel(255)
        #endif
        
        SaneConfig.makeConfigAvailableToSaneLib()
    }
    
    // MARK: Properties
    @objc public weak var delegate: SaneDelegate?
    @objc public private(set) var saneInitError: String?
    @objc public private(set) var isUpdatingDevices: Bool {
        get {
            var value = false
            lockIsUpdatingDevices.lock()
            value = internalIsUpdatingDevices
            lockIsUpdatingDevices.unlock()
            return value
        }
        set {
            lockIsUpdatingDevices.lock()
            internalIsUpdatingDevices = newValue
            lockIsUpdatingDevices.unlock()
            
            DispatchQueue.main.async {
                if newValue {
                    self.delegate?.saneDidStartUpdatingDevices(self)
                } else {
                    self.delegate?.saneDidEndUpdatingDevices(self)
                }
            }
        }
    }
    
    // MARK: Private properties
    private var thread: Thread!
    private var saneStarted = false
    private let translation = Translation(locale: Locale.current)
    private var openedDevices = [String: NSValue]()
    private var stopScanOperation = false
    private let lockIsUpdatingDevices = NSLock()
    private var internalIsUpdatingDevices: Bool = false
    
    private var lockQueueBlocks = NSLock()
    private var queuedBlocks = [() -> ()]()

    // MARK: Translations
    @objc public func translation(for key: String) -> String {
        return translation?.translation(for: key) ?? key
    }
    
    // MARK: Configuration
    @objc public var configuration: SaneConfig = SaneConfig.restored() ?? SaneConfig()
}

// MARK: Threading
extension Sane {
    @objc private func threadKeepAlive() {
        let runloop = RunLoop.current
        runloop.add(NSMachPort(), forMode: .default)
        while runloop.run(mode: .default, before: Date.distantFuture) {
            //run loop spinned ones
        }
    }

    @objc private func runOnSaneThread(block: (() -> ())?) {
        self.ss_perform(block, on: thread)
    }
}

// MARK: Benchmarking
extension Sane {
    private static func logTime<T>(function: String = #function, block: () -> T) -> T {
        let startDate = Date()
        let returnValue = block()
        let interval = Date().timeIntervalSince(startDate)
        if interval > 1 {
            print(function + ": " + String(interval) + "s")
        }
        return returnValue
    }
}

// MARK: Sane start and stop
extension Sane {
    private func startSane() {
        runOnSaneThread {
            guard !self.saneStarted else { return }
            
            self.openedDevices = [String: NSValue]()
            self.isUpdatingDevices = false
            
            // TODO: auth callback!
            let s = sane_init(nil, SaneAuthenticationCallback(deviceName:username:password:))
            
            if s != SANE_STATUS_GOOD {
                self.saneInitError = NSStringFromSANEStatus(s)
            } else {
                self.saneInitError = nil
            }
            
            self.saneStarted = (s == SANE_STATUS_GOOD);
            print("SANE started:", self.saneStarted)
        }
    }
    
    private func stopSane() {
        runOnSaneThread {
            guard self.saneStarted else { return }
            
            self.openedDevices = [String: NSValue]()
            self.isUpdatingDevices = false
            sane_exit()
            
            self.saneStarted = false
            print("SANE stopped")
        }
    }
}

// MARK: Authentication
private func SaneAuthenticationCallback(deviceName: SANE_String_Const?, username: UnsafeMutablePointer<SANE_Char>?, password: UnsafeMutablePointer<SANE_Char>?) {
    let semaphore = DispatchSemaphore(value: 0)
    var auth: DeviceAuthentication? = nil
    
    // this method will be called from the SANE thread, let's escape it to call the delegate
    DispatchQueue.main.async {
        Sane.shared.delegate?.saneNeedsAuth(Sane.shared, for: NSStringFromSaneString(deviceName), completion: { (authentication) in
            auth = authentication
            _ = semaphore.signal()
        })
    }
    
    // let's wait for the delegate to answer
    _ = semaphore.wait(timeout: .distantFuture)
    
    // TODO: handle SANE_MAX_USERNAME_LEN and SANE_MAX_PASSWORD_LEN
    // TODO: need to cache?
    username?.pointee = auth?.username?.cString(using: .utf8)?.first ?? 0
    password?.pointee = auth?.password?.cString(using: .utf8)?.first ?? 0
}

extension Sane {
    // MARK: Device management
    @objc public func updateDevices(completion: @escaping (_ devices: [Device]?, _ error: Error?) -> ()) {
        runOnSaneThread {
            guard !self.isUpdatingDevices else { return }
            
            self.startSane()
            self.isUpdatingDevices = true
            
            var rawDevices: UnsafeMutablePointer<UnsafePointer<SANE_Device>?>? = nil
            
            let s: SANE_Status = Sane.logTime {
                sane_get_devices(&rawDevices, SANE_FALSE)
            }
            
            guard s == SANE_STATUS_GOOD else {
                self.isUpdatingDevices = false
                DispatchQueue.main.async {
                    completion(nil, SaneError.fromStatus(s))
                }
                return
            }
            
            var devices = [Device]()
            var i = 0
            
            while let rawDevice = rawDevices?.advanced(by: i).pointee {
                devices.append(Device(cDevice: rawDevice.pointee))
                i += 1
            }
            
            self.isUpdatingDevices = false
            
            DispatchQueue.main.async {
                completion(devices, nil)
            }
            
            self.stopSane()
        }
    }
    
    @objc public func openDevice(_ device: Device, completion: @escaping (Error?) -> ()) {
        self.openDevice(device, mainThread: Thread.isMainThread, completion: completion)
    }

    private func openDevice(_ device: Device, mainThread: Bool, completion: @escaping (Error?) -> ()) {
        runOnSaneThread {
            guard self.openedDevices[device.name] == nil else { return }
            self.startSane()
            
            var h: SANE_Handle? = nil
            
            let s: SANE_Status = Sane.logTime {
                sane_open(device.name.cString(using: .utf8), &h)
            }
            
            guard s == SANE_STATUS_GOOD else {
                if mainThread {
                    DispatchQueue.main.async {
                        completion(SaneError.fromStatus(s))
                    }
                }
                else {
                    completion(SaneError.fromStatus(s))
                }
                return
            }
            
            self.openedDevices[device.name] = NSValue(pointer: h)
            if mainThread {
                DispatchQueue.main.async { completion(nil) }
            } else {
                completion(nil)
            }
        }
    }
    
    @objc public func closeDevice(_ device: Device) {
        runOnSaneThread {
            guard let handle: SANE_Handle = self.openedDevices[device.name]?.pointerValue else { return }
            Sane.logTime { sane_close(handle) }
            self.openedDevices.removeValue(forKey: device.name)
            
            if self.openedDevices.isEmpty {
                self.stopSane()
            }
        }
    }
    
    @objc public func isDeviceOpened(_ device: Device) -> Bool {
        return self.openedDevices[device.name]?.pointerValue != nil
    }
}

extension Sane {
    // MARK: Options
    @objc public func listOptions(for device: Device, completion: (() -> ())?) {
        self.listOptions(for: device, mainThread: Thread.isMainThread, completion: completion)
    }
    
    private func listOptions(for device: Device, mainThread: Bool, completion: (() -> ())?) {
        runOnSaneThread {
            guard let handle: SANE_Handle = self.openedDevices[device.name]?.pointerValue else { return }
            
            var options = [DeviceOption]()
            
            Sane.logTime {
                
                var count = SANE_Int(0)
                
                // needed for sane to update the value of the option count
                var descriptor = sane_get_option_descriptor(handle, 0);
                
                let s = sane_control_option(handle, 0, SANE_ACTION_GET_VALUE, &count, nil);
                guard s == SANE_STATUS_GOOD else {
                    if mainThread { DispatchQueue.main.async { completion?() } }
                    else { completion?() }
                    return
                }
                
                for i in 1..<count {
                    descriptor = sane_get_option_descriptor(handle, i)
                    options.append(DeviceOption.bestOption(cOpt: descriptor!.pointee, index: Int(i), device: device))
                }
                
                options.forEach { $0.refreshValue(nil) }
                
            }
            
            device.options = options
            
            if mainThread { DispatchQueue.main.async { completion?() } }
            else { completion?() }
        }
    }
    
    public func valueForOption(_ option: DeviceOption, completion: @escaping (_ value: Any?, _ error: Error?) -> ()) {
        self.valueForOption(option, mainThread: Thread.isMainThread, completion: completion)
    }
    
    private func valueForOption(_ option: DeviceOption, mainThread: Bool, completion: @escaping (_ value: Any?, _ error: Error?) -> ()) {
        guard let handle: SANE_Handle = self.openedDevices[option.device.name]?.pointerValue else {
            completion(nil, SaneError.deviceNotOpened)
            return
        }
        
        guard option.type != SANE_TYPE_GROUP else {
            completion(nil, SaneError.getValueForGroupType)
            return
        }
        
        guard option.type != SANE_TYPE_BUTTON else {
            completion(nil, SaneError.getValueForButtonType)
            return
        }
        
        guard !option.capInactive else {
            completion(nil, SaneError.getValueForInactiveOption)
            return
        }
        
        runOnSaneThread {
            // TODO: use something else than malloc
            let value = malloc(Int(option.size))!
            
            let s = Sane.logTime { sane_control_option(handle, SANE_Int(option.index), SANE_ACTION_GET_VALUE, value, nil) }
            
            guard s == SANE_STATUS_GOOD else {
                if mainThread { DispatchQueue.main.async { completion(nil, SaneError.fromStatus(s)) } }
                else { completion(nil, SaneError.fromStatus(s)) }
                return
            }
            
            var castedValue: Any?
            if option.type == SANE_TYPE_BOOL {
                castedValue = value.bindMemory(to: SANE_Bool.self, capacity: 1).pointee
            }
            else if option.type == SANE_TYPE_INT || option.type == SANE_TYPE_FIXED {
                castedValue = value.bindMemory(to: SANE_Int.self, capacity: 1).pointee
            }
            else if option.type == SANE_TYPE_STRING {
                castedValue = String(cString: value.bindMemory(to: SANE_Char.self, capacity: Int(option.size)))
            }
            
            free(value)
            
            if mainThread { DispatchQueue.main.async { completion(castedValue, nil) } }
            else { completion(castedValue, nil) }
        }
    }

    private func setCropArea(_ cropArea: CGRect, useAuto: Bool, device: Device, mainThread: Bool, completion: ((_ reloadAllOptions: Bool, _ error: Error?) -> ())?) {
        guard let handle: SANE_Handle = self.openedDevices[device.name]?.pointerValue else {
            completion?(false, SaneError.deviceNotOpened)
            return
        }

        runOnSaneThread {
            var finalReloadAllOptions = false
            var finalError: Error? = nil
            
            let stdOptions = [SaneStandardOption.areaTopLeftX, .areaTopLeftY, .areaBottomRightX, .areaBottomRightY]
            let values = [cropArea.minX, cropArea.minY, cropArea.maxX, cropArea.maxY]
            
            for (option, value) in zip(stdOptions, values) {
                let option = device.standardOption(for: option) as! DeviceOptionNumber
                self.setValueForOption(value: value, auto: false, option: option, completion: { (reloadAllOptions, error) in
                    finalReloadAllOptions = finalReloadAllOptions || reloadAllOptions
                    finalError = error
                })
                
                guard finalError == nil else { break }
            }
            
            if mainThread { DispatchQueue.main.async { completion?(finalReloadAllOptions, finalError) } }
            else { completion?(finalReloadAllOptions, finalError) }
        }
    }

    public func setValueForOption(value: Any?, auto: Bool, option: DeviceOption, completion: ((_ shouldReloadAllOptions: Bool, _ error: Error?) -> ())?) {
        self.setValueForOption(value: value, auto: auto, option: option, mainThread: Thread.isMainThread, completion: completion)
    }

    private func setValueForOption(value: Any?, auto: Bool, option: DeviceOption, mainThread: Bool, completion: ((_ shouldReloadAllOptions: Bool, _ error: Error?) -> ())?) {
        guard let handle: SANE_Handle = self.openedDevices[option.device.name]?.pointerValue else {
            completion?(false, SaneError.deviceNotOpened)
            return
        }
        
        guard option.type != SANE_TYPE_GROUP else {
            completion?(false, SaneError.setValueForGroupType)
            return
        }
        
        runOnSaneThread {
            var byteValue: UnsafeMutableRawPointer? = nil
            
            if !auto {
                // TODO: an option should be able to convert its value from/to raw bytes
                byteValue = malloc(Int(option.size))
                if option.type == SANE_TYPE_BOOL {
                    byteValue?.bindMemory(to: SANE_Bool.self, capacity: 1).pointee = ((value as? Bool) ?? false) ? 1 : 0
                }
                else if option.type == SANE_TYPE_INT {
                    byteValue?.bindMemory(to: SANE_Int.self, capacity: 1).pointee = (value as? SANE_Int) ?? 0
                }
                else if option.type == SANE_TYPE_FIXED {
                    byteValue?.bindMemory(to: SANE_Fixed.self, capacity: 1).pointee = SaneFixedFromDouble((value as? Double) ?? 0)
                }
                else if option.type == SANE_TYPE_STRING {
                    let cString = ((value as? String) ?? "").cString(using: String.Encoding.utf8) ?? []
                    let size = min(cString.count, Int(option.size))
                    byteValue?.bindMemory(to: SANE_Char.self, capacity: Int(option.size)).assign(from: cString, count: size)
                    // TODO: fix some strings that are not working (e.g.: scan mode = Grey or Colour)
                }
            }
                
            var info: SANE_Int = 0
            let status: SANE_Status
            
            if auto {
                status = Sane.logTime { sane_control_option(handle, SANE_Int(option.index), SANE_ACTION_SET_AUTO, nil, &info) }
            } else {
                status = Sane.logTime { sane_control_option(handle, SANE_Int(option.index), SANE_ACTION_SET_VALUE, byteValue, &info) }
            }
            
            let reloadAllOptions = ((info & SANE_INFO_RELOAD_OPTIONS) > 0) || ((info & SANE_INFO_RELOAD_PARAMS) > 0)
            var updatedValue = false
            
            if status == SANE_STATUS_GOOD && !auto {
                // TODO: was using byteValue before, needed?
                if option.type == SANE_TYPE_BOOL {
                    let castedOption = option as! DeviceOptionBool
                    castedOption.value = (value as? Bool) ?? false
                    updatedValue = true
                }
                else if option.type == SANE_TYPE_INT {
                    let castedOption = option as! DeviceOptionNumber
                    castedOption.value = NSNumber(value: (value as? Int) ?? 0)
                    updatedValue = true
                }
                else if option.type == SANE_TYPE_FIXED {
                    let castedOption = option as! DeviceOptionNumber
                    // [castedOption setValue:@(SANE_UNFIX(((SANE_Fixed *)byteValue)[0]))];
                    castedOption.value = NSNumber(value: (value as? Double) ?? 0)
                    updatedValue = true
                }
                else if option.type == SANE_TYPE_STRING {
                    let castedOption = option as! DeviceOptionString
                    castedOption.value = (value as? String)
                    updatedValue = true
                }
            }
            
            if !auto && (option.type == SANE_TYPE_BOOL || option.type == SANE_TYPE_INT || option.type == SANE_TYPE_FIXED) {
                free(byteValue)
            }
            
            if status == SANE_STATUS_GOOD && !reloadAllOptions && !updatedValue {
                option.refreshValue(nil)
            }
            
            let error: Error? = status == SANE_STATUS_GOOD ? nil : SaneError.fromStatus(status)
            if mainThread { DispatchQueue.main.async { completion?(reloadAllOptions, error) } }
            else { completion?(reloadAllOptions, error) }
        }
    }
}

extension Sane {
    // MARK: Scan
    @objc public func preview(device: Device, progress: ((_ progress: Float, _ incompleteImage: UIImage?) -> ())?, completion: @escaping (_ image: UIImage?, _ error: Error?) -> ()) {
        self.preview(device: device, mainThread: Thread.isMainThread, progress: progress, completion: completion)
    }
    
    private func preview(device: Device, mainThread: Bool, progress: ((_ progress: Float, _ incompleteImage: UIImage?) -> ())?, completion: @escaping (_ image: UIImage?, _ error: Error?) -> ()) {
        
        guard let handle: SANE_Handle = self.openedDevices[device.name]?.pointerValue else {
            completion(nil, SaneError.deviceNotOpened)
            return
        }
        
        runOnSaneThread {
            var oldOptions = [SaneStandardOption: Any?]()
            
            if let optionPreview = device.standardOption(for: .preview) {
                self.setValueForOption(value: true, auto: false, option: optionPreview, completion: nil)
            }
            else {
                var stdOptions = [SaneStandardOption.resolutionX, .resolutionY,
                                  .areaTopLeftX, .areaTopLeftY,
                                  .areaBottomRightX, .areaBottomRightY]

                if self.configuration.previewWithAutoColorMode {
                    stdOptions.append(.colorMode)
                }
                
                stdOptions.forEach { stdOption in
                    guard let option = device.standardOption(for: stdOption) else { return }
                    let bestValue = stdOption.bestPreviewValue
                    
                    var newValue: Any?
                    var useAuto = false
                    
                    if let castedOption = option as? DeviceOptionNumber {
                        newValue = castedOption.bestValueForPreview
                        useAuto  = (bestValue == .auto)
                        oldOptions[stdOption] = castedOption.value
                    }
                    else if let castedOption = option as? DeviceOptionString {
                        if bestValue != .auto {
                            // TODO: raise error?
                            print("Unsupported configuration : option", option.identifier, "is a string but cannot be set to auto")
                            return
                        }
                        
                        useAuto = true
                        oldOptions[stdOption] = castedOption.value
                    }
                    else {
                        // TODO: raise error?
                        print("Unsupported configuration: option type for", option.identifier, "is not supported");
                        return
                    }
                    
                    self.setValueForOption(value: newValue, auto: useAuto, option: option, completion: { (reloadAll, error) in
                        if reloadAll {
                            self.listOptions(for: device, completion: nil)
                        }
                    });
                }

            }

            
            var previewImage: UIImage?
            var previewError: Error?

            self.scan(device: device, useScanCropArea: false, mainThread: false, progress: progress, completion: { (image, _, error) in
                previewImage = image
                previewError = error
            })
            
            if let optionPreview = device.standardOption(for: .preview) {
                self.setValueForOption(value: false, auto: false, option: optionPreview, completion: nil)
            }
            else {
                oldOptions.forEach { (stdOption, value) in
                    guard let option = device.standardOption(for: stdOption) else { return }
                    self.setValueForOption(value: value, auto: false, option: option, mainThread: false, completion: { (reloadAll, error) in
                        if reloadAll {
                            self.listOptions(for: device, mainThread: false, completion: nil)
                        }
                    })
                }
            }
            
            device.lastPreviewImage = previewImage
            if mainThread { DispatchQueue.main.sync { completion(previewImage, previewError) } }
            else { completion(previewImage, previewError) }
        }
    }
    
    public func scan(device: Device, progress: ((_ progress: Float, _ incompleteImage: UIImage?) -> ())?, completion: ((_ image: UIImage?, _ parameters: ScanParameters?, _ error: Error?) -> ())?) {
        self.scan(device: device, useScanCropArea: true, mainThread: Thread.isMainThread, progress: progress, completion: completion)
    }
    
    private func scan(device: Device, useScanCropArea: Bool, mainThread: Bool, progress: ((_ progress: Float, _ incompleteImage: UIImage?) -> ())?, completion: ((_ image: UIImage?, _ parameters: ScanParameters?, _ error: Error?) -> ())?) {
        
        guard let handle: SANE_Handle = self.openedDevices[device.name]?.pointerValue else {
            completion?(nil, nil, SaneError.deviceNotOpened)
            return
        }

        runOnSaneThread {
            self.stopScanOperation = false
            
            if device.canCrop && useScanCropArea {
                self.setCropArea(device.cropArea, useAuto: false, device: device, mainThread: false, completion: nil)
            }
            
            var status = Sane.logTime { sane_start(handle) }
            
            guard status == SANE_STATUS_GOOD else {
                if mainThread { DispatchQueue.main.async { completion?(nil, nil, SaneError.fromStatus(status)) } }
                else { completion?(nil, nil, SaneError.fromStatus(status)) }
                return
            }
            
            status = sane_set_io_mode(handle, SANE_FALSE)
            
            guard status == SANE_STATUS_GOOD else {
                if mainThread { DispatchQueue.main.async { completion?(nil, nil, SaneError.fromStatus(status)) } }
                else { completion?(nil, nil, SaneError.fromStatus(status)) }
                return
            }
            
            var estimatedParams = SANE_Parameters()
            status = sane_get_parameters(handle, &estimatedParams)
            
            guard status == SANE_STATUS_GOOD else {
                if mainThread { DispatchQueue.main.async { completion?(nil, nil, SaneError.fromStatus(status)) } }
                else { completion?(nil, nil, SaneError.fromStatus(status)) }
                return
            }

            let estimatedParameters = ScanParameters(cParams: estimatedParams)
            
            var data = Data(capacity: estimatedParameters.fileSize + 1)
            let bufferMaxSize = max(100 * 1000, estimatedParameters.fileSize / 100)

            let buffer = malloc(Int(bufferMaxSize))!.bindMemory(to: UInt8.self, capacity: bufferMaxSize)
            var bufferActualSize: SANE_Int = 0
            var parameters: ScanParameters?
            
            let prevLogLevel = SaneGetLogLevel()
            SaneSetLogLevel(0)
            
            // generate incomplete image preview every 2%
            var progressForLastIncompletePreview: Float = 0
            var incompletePreviewStep: Float = 0.02
            
            while status == SANE_STATUS_GOOD {
                
                if let parameters = parameters, let progress = progress, parameters.fileSize > 0 {
                    let percent = Float(data.count) / Float(parameters.fileSize)
                    
                    if self.configuration.showIncompleteScanImages {
                        if percent > progressForLastIncompletePreview + incompletePreviewStep {
                            progressForLastIncompletePreview = percent
                            
                            DispatchQueue.main.async {
                                // image creation needs to be done on main thread
                                let incompleteImage = try? UIImage.sy_imageFromIncompleteSane(data: data, parameters: parameters)
                                progress(percent, incompleteImage)
                            }
                        }
                    }
                    else {
                        DispatchQueue.main.async {
                            progress(percent, nil)
                        }
                    }
                }
                
                data.append(buffer, count: Int(bufferActualSize))
                //[fileHandle writeData:[NSData dataWithBytes:buffer length:bufferActualSize]];
                
                if self.stopScanOperation {
                    self.stopScanOperation = false
                    sane_cancel(handle)
                }
                
                status = sane_read(handle, buffer, SANE_Int(bufferMaxSize), &bufferActualSize)
                
                if parameters == nil {
                    var params = SANE_Parameters()
                    sane_get_parameters(handle, &params)
                    parameters = ScanParameters(cParams: params)
                }
                
                // lineart requires inverting pixel values
                if parameters?.currentlyAcquiredChannel == SANE_FRAME_GRAY && parameters?.depth == 1 {
                    (0..<Int(bufferActualSize)).forEach { i in
                        buffer[i] = ~buffer[i]
                    }
                }
            }
            
            free(buffer)
            
            SaneSetLogLevel(prevLogLevel)
            
            guard status == SANE_STATUS_EOF, parameters != nil else {
                if mainThread { DispatchQueue.main.async { completion?(nil, nil, SaneError.fromStatus(status)) } }
                else { completion?(nil, nil, SaneError.fromStatus(status)) }
                return
            }

            do {
                let image = try UIImage.sy_imageFromSane(source: UIImage.SaneSource.data(data), parameters: parameters!)

                if mainThread { DispatchQueue.main.async { completion?(image, parameters, nil) } }
                else { completion?(image, parameters, nil) }
            }
            catch {
                if mainThread { DispatchQueue.main.async { completion?(nil, nil, error) } }
                else { completion?(nil, nil, error) }
            }
        }
    }
    
    @objc public func cancelCurrentScan() {
        self.stopScanOperation = true
    }
}
