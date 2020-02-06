//
//  Sane.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation
import UIKit

public protocol SaneDelegate: NSObjectProtocol {
    func saneDidStartUpdatingDevices(_ sane: Sane)
    func saneDidEndUpdatingDevices(_ sane: Sane)
    func saneNeedsAuth(_ sane: Sane, for device: String?, completion: @escaping (DeviceAuthentication?) -> ())
}

public class Sane: NSObject {
    
    // MARK: Init
    public static let shared = Sane()
    
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
    public weak var delegate: SaneDelegate?
    public private(set) var saneInitError: String?
    public private(set) var isUpdatingDevices: Bool {
        get {
            var value = false
            lockIsUpdatingDevices.lock()
            value = internalIsUpdatingDevices
            lockIsUpdatingDevices.unlock()
            return value
        }
        set {
            if internalIsUpdatingDevices == newValue {
                return
            }
            
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
    private var openedDevices = [String: NSValue]()
    private var stopScanOperation = false
    private let lockIsUpdatingDevices = NSLock()
    private var internalIsUpdatingDevices: Bool = false
    
    private var lockQueueBlocks = NSLock()
    private var queuedBlocks = [() -> ()]()

    // MARK: Configuration
    public var configuration: SaneConfig = SaneConfig.restored() ?? SaneConfig()
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
        self.saneSwift_perform(block, on: thread)
    }
    
    private static func runOn(mainThread: Bool, block: @escaping () -> ()) {
        if mainThread {
            DispatchQueue.main.async { block() }
        }
        else {
            block()
        }
    }
}

// MARK: Benchmarking
extension Sane {
    private static func logTime<T>(ifOver minReportDuration: TimeInterval = 1, function: String = #function, block: () -> T) -> T {
        let startDate = Date()
        let returnValue = block()
        let interval = Date().timeIntervalSince(startDate)
        if interval > minReportDuration {
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
            
            let s = sane_init(nil, SaneAuthenticationCallback(deviceName:username:password:))
            
            if s != SANE_STATUS_GOOD {
                self.saneInitError = s.description
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
    
    var deviceString: String?
    var md5string: String?
    
    if let components = deviceName?.asString()?.components(separatedBy: "$MD5$") {
        deviceString = components.first
        md5string = components.count > 1 ? components.last : nil
    }
    
    // this method will be called from the SANE thread, let's escape it to call the delegate
    DispatchQueue.main.async {
        Sane.shared.delegate?.saneNeedsAuth(Sane.shared, for: deviceString, completion: { (authentication) in
            auth = authentication
            _ = semaphore.signal()
        })
    }
    
    // let's wait for the delegate to answer
    _ = semaphore.wait(timeout: .distantFuture)
    
    // update return values
    if let cUsername = auth?.username(splitToMaxLength: true)?.cString(using: .utf8) {
        username?.initialize(from: cUsername, count: cUsername.count)
    }
    if let cPassword = auth?.password(splitToMaxLength: true, md5DigestUsing: md5string)?.cString(using: .utf8) {
        password?.initialize(from: cPassword, count: cPassword.count)
    }
}

extension Sane {
    // MARK: Device management
    public func updateDevices(completion: @escaping (_ devices: [Device]?, _ error: Error?) -> ()) {
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
                Sane.runOn(mainThread: true, block: {
                    completion(nil, SaneError.fromStatus(s))
                })
                return
            }
            
            var devices = [Device]()
            var i = 0
            
            while let rawDevice = rawDevices?.advanced(by: i).pointee {
                devices.append(Device(cDevice: rawDevice.pointee))
                i += 1
            }
            
            self.isUpdatingDevices = false
            
            Sane.runOn(mainThread: true, block: {
                completion(devices, nil)
            })
            
            self.stopSane()
        }
    }
    
    public func openDevice(_ device: Device, completion: @escaping (Error?) -> ()) {
        let mainThread = Thread.isMainThread
        
        runOnSaneThread {
            guard self.openedDevices[device.name] == nil else { return }
            self.startSane()
            
            var h: SANE_Handle? = nil
            
            let s: SANE_Status = Sane.logTime {
                sane_open(device.name.cString(using: .utf8), &h)
            }
            
            if s == SANE_STATUS_GOOD {
                self.openedDevices[device.name] = NSValue(pointer: h)
            }

            Sane.runOn(mainThread: mainThread, block: {
                completion(SaneError.fromStatus(s))
            })
        }
    }
    
    public func closeDevice(_ device: Device) {
        runOnSaneThread {
            guard let handle: SANE_Handle = self.openedDevices[device.name]?.pointerValue else { return }
            Sane.logTime { sane_close(handle) }
            self.openedDevices.removeValue(forKey: device.name)
            
            if self.openedDevices.isEmpty {
                self.stopSane()
            }
        }
    }
    
    public func isDeviceOpened(_ device: Device) -> Bool {
        return self.openedDevices[device.name]?.pointerValue != nil
    }
}

extension Sane {
    // MARK: Options
    public func listOptions(for device: Device, completion: (() -> ())?) {
        let mainThread = Thread.isMainThread
        
        runOnSaneThread {
            guard let handle: SANE_Handle = self.openedDevices[device.name]?.pointerValue else { return }
            
            var options = [DeviceOption]()
            
            Sane.logTime {
                
                var count = SANE_Int(0)
                
                // needed for sane to update the value of the option count
                var descriptor = sane_get_option_descriptor(handle, 0);
                
                let s = sane_control_option(handle, 0, SANE_ACTION_GET_VALUE, &count, nil);
                guard s == SANE_STATUS_GOOD else {
                    Sane.runOn(mainThread: mainThread) { completion?() }
                    return
                }
                
                for i in 1..<count {
                    descriptor = sane_get_option_descriptor(handle, i)
                    options.append(DeviceOption.typedOption(cOption: descriptor!.pointee, index: Int(i), device: device))
                }
                
                options.forEach { $0.refreshValue(nil) }
            }
            
            device.options = options
            
            Sane.runOn(mainThread: mainThread) { completion?() }
        }
    }
    
    public func valueForOption<V, T: DeviceOptionTyped<V>>(_ option: T, completion: @escaping (_ value: V?, _ error: Error?) -> ()) {
        let mainThread = Thread.isMainThread
        
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
        
        guard option.capabilities.isActive else {
            completion(nil, SaneError.getValueForInactiveOption)
            return
        }
        
        runOnSaneThread {
            let bytes = malloc(option.size)!
            
            let s = Sane.logTime { sane_control_option(handle, SANE_Int(option.index), SANE_ACTION_GET_VALUE, bytes, nil) }
            let value = option.valueForBytes(bytes)
            free(bytes)

            guard s == SANE_STATUS_GOOD else {
                Sane.runOn(mainThread: mainThread) { completion(nil, SaneError.fromStatus(s)) }
                return
            }
            
            Sane.runOn(mainThread: mainThread) { completion(value, nil) }
        }
    }

    private func setCropArea(_ cropArea: CGRect, useAuto: Bool, device: Device, completion: ((_ error: Error?) -> ())?) {
        let mainThread = Thread.isMainThread
        
        guard openedDevices[device.name]?.pointerValue != nil else {
            completion?(SaneError.deviceNotOpened)
            return
        }

        runOnSaneThread {
            var finalError: Error? = nil
            
            let stdOptions = [SaneStandardOption.areaTopLeftX, .areaTopLeftY, .areaBottomRightX, .areaBottomRightY]
            let values = [cropArea.minX, cropArea.minY, cropArea.maxX, cropArea.maxY]
            
            for (option, value) in zip(stdOptions, values) {
                if let option = device.standardOption(for: option) as? DeviceOptionInt {
                    self.updateOption(option, with: .value(Int(value)), completion: { (error) in
                        finalError = error
                    })
                }
                if let option = device.standardOption(for: option) as? DeviceOptionFixed {
                    self.updateOption(option, with: .value(Double(value)), completion: { (error) in
                        finalError = error
                    })
                }

                guard finalError == nil else { break }
            }
            
            Sane.runOn(mainThread: mainThread) { completion?(finalError) }
        }
    }

    public func updateOption<V, T: DeviceOptionTyped<V>>(_ option: T, with value: DeviceOptionNewValue<V>, completion: ((_ error: Error?) -> ())?) {
        let mainThread = Thread.isMainThread
        
        guard let handle: SANE_Handle = self.openedDevices[option.device.name]?.pointerValue else {
            completion?(SaneError.deviceNotOpened)
            return
        }
        
        guard option.type != SANE_TYPE_GROUP else {
            completion?(SaneError.setValueForGroupType)
            return
        }
        
        // Don't try to ignore changes if option.value is already the same as the value we want: we may be using an outdated option, since
        // options objects are recreated each time we use listOptions(for:completion:), and that old option would not have the proper value.
        // Finding the updated option's value would use a lot of code for not so much time saved. In a future version we could update a option
        // object instead so that we are sure the option is always up to date, but that seems a bit too much to be honest. The time savings we
        // actually need are down there with the SaneInfo bits.
        
        runOnSaneThread {
            var byteValue: UnsafeMutableBufferPointer<UInt8>? = nil
            
            if case let .value(value) = value {
                let data = option.bytesForValue(value).subarray(maxCount: option.size)
                
                byteValue = UnsafeMutableBufferPointer<UInt8>.allocate(capacity: option.size)
                _ = data.copyBytes(to: byteValue!)
            }
                
            var info: SANE_Int = 0
            let status: SANE_Status
            
            if case .auto = value {
                status = Sane.logTime { sane_control_option(handle, SANE_Int(option.index), SANE_ACTION_SET_AUTO, nil, &info) }
            } else {
                status = Sane.logTime { sane_control_option(handle, SANE_Int(option.index), SANE_ACTION_SET_VALUE, byteValue?.baseAddress, &info) }
            }

            if byteValue != nil {
                byteValue?.deallocate()
            }
            
            if status == SANE_STATUS_GOOD {
                if SaneInfo(rawValue: info).contains(.reloadOptions) {
                    // this is absolutely needed, because if the option declares it needs to reload other options, setting any option before
                    // doing so will result in SANE_STATUS_INVAL. So we make sure each changes that needs to reload options *does* reload them
                    self.listOptions(for: option.device, completion: nil)
                }
                else {
                    // some changes can be accepted but inexact, or we used an auto value and need to figure out the value that is actually used
                    option.refreshValue(nil)
                }
            }
            
            Sane.runOn(mainThread: mainThread) { completion?(SaneError.fromStatus(status)) }
        }
    }
    
    typealias RestoreBlock = () -> ()
    internal func updateOptionForPreview<V, T: DeviceOptionTyped<V>>(_ option: T) -> RestoreBlock {
        guard Thread.current == self.thread else {
            fatalError("This method should only be used on the SANE thread")
        }
        
        let prevValue = option.value
        
        let restoreBlock = {
            Sane.shared.updateOption(option, with: .value(prevValue), completion: nil)
        }
        
        switch option.bestPreviewValue {
        case .auto:
            Sane.shared.updateOption(option, with: .auto, completion: nil)
            return restoreBlock
        case .value(let value):
            Sane.shared.updateOption(option, with: .value(value), completion: nil)
            return restoreBlock
        case .none:
            return {}
        }
    }
}

extension Sane {
    // MARK: Scan
    public func preview(device: Device, progress: ((_ progress: Float, _ incompleteImage: UIImage?) -> ())?, completion: @escaping (_ image: UIImage?, _ error: Error?) -> ()) {
        let mainThread = Thread.isMainThread
        
        guard openedDevices[device.name]?.pointerValue != nil else {
            completion(nil, SaneError.deviceNotOpened)
            return
        }
        
        runOnSaneThread {
            var restoreBlocks = [(RestoreBlock)]()
            
            if let optionPreview = device.standardOption(for: .preview) as? DeviceOptionBool {
                self.updateOption(optionPreview, with: .value(true), completion: nil)
            }
            else {
                var stdOptions = [SaneStandardOption.resolution, .resolutionX, .resolutionY,
                                  .areaTopLeftX, .areaTopLeftY,
                                  .areaBottomRightX, .areaBottomRightY]
                
                if self.configuration.previewWithAutoColorMode {
                    stdOptions.append(.colorMode)
                    stdOptions.append(.imageIntensity)
                }
                
                stdOptions.forEach { stdOption in
                    guard let option = device.standardOption(for: stdOption) else { return }
                    
                    if let option = option as? DeviceOptionBool {
                        restoreBlocks.append(self.updateOptionForPreview(option))
                    }
                    else if let option = option as? DeviceOptionInt {
                        restoreBlocks.append(self.updateOptionForPreview(option))
                    }
                    else if let option = option as? DeviceOptionFixed {
                        restoreBlocks.append(self.updateOptionForPreview(option))
                    }
                    else if let option = option as? DeviceOptionString {
                        restoreBlocks.append(self.updateOptionForPreview(option))
                    }
                    else {
                        fatalError("Unsupported configuration: option type for \(option.identifier ?? "<no id>") is not supported")
                    }
                }
            }

            
            var previewImage: UIImage?
            var previewError: Error?

            self.scan(device: device, useScanCropArea: false, progress: progress, completion: { (image, _, error) in
                previewImage = image
                previewError = error
            })
            
            if let optionPreview = device.standardOption(for: .preview) as? DeviceOptionBool {
                self.updateOption(optionPreview, with: .value(false), completion: nil)
            }
            else {
                restoreBlocks.forEach { $0() }
            }
            
            device.lastPreviewImage = previewImage
            Sane.runOn(mainThread: mainThread) { completion(previewImage, previewError) }
        }
    }
    
    public func scan(device: Device, useScanCropArea: Bool = true, progress: ((_ progress: Float, _ incompleteImage: UIImage?) -> ())?, completion: ((_ image: UIImage?, _ parameters: ScanParameters?, _ error: Error?) -> ())?) {
        
        let mainThread = Thread.isMainThread

        guard let handle: SANE_Handle = self.openedDevices[device.name]?.pointerValue else {
            completion?(nil, nil, SaneError.deviceNotOpened)
            return
        }

        runOnSaneThread {
            self.stopScanOperation = false
            
            if device.canCrop && useScanCropArea {
                self.setCropArea(device.cropArea, useAuto: false, device: device, completion: nil)
            }
            
            var status = Sane.logTime { sane_start(handle) }
            
            guard status == SANE_STATUS_GOOD else {
                Sane.runOn(mainThread: mainThread) { completion?(nil, nil, SaneError.fromStatus(status)) }
                return
            }
            
            status = sane_set_io_mode(handle, SANE_FALSE)
            
            guard status == SANE_STATUS_GOOD else {
                Sane.runOn(mainThread: mainThread) { completion?(nil, nil, SaneError.fromStatus(status)) }
                return
            }
            
            var estimatedParams = SANE_Parameters()
            status = sane_get_parameters(handle, &estimatedParams)
            
            guard status == SANE_STATUS_GOOD else {
                Sane.runOn(mainThread: mainThread) { completion?(nil, nil, SaneError.fromStatus(status)) }
                return
            }

            let estimatedParameters = ScanParameters(cParams: estimatedParams)
            
            var data = Data(capacity: estimatedParameters.fileSize + 1)
            let bufferMaxSize = max(100 * 1000, estimatedParameters.fileSize / 100)

            let buffer = malloc(bufferMaxSize)!.bindMemory(to: UInt8.self, capacity: bufferMaxSize)
            var bufferActualSize: SANE_Int = 0
            var parameters: ScanParameters?
            
            let prevLogLevel = SaneGetLogLevel()
            SaneSetLogLevel(0)
            
            // generate incomplete image preview every 2%
            var progressForLastIncompletePreview: Float = 0
            let incompletePreviewStep: Float = 0.02
            
            while status == SANE_STATUS_GOOD {
                
                if let parameters = parameters, let progress = progress, parameters.fileSize > 0 {
                    let percent = Float(data.count) / Float(parameters.fileSize)
                    
                    if self.configuration.showIncompleteScanImages {
                        if percent > progressForLastIncompletePreview + incompletePreviewStep {
                            progressForLastIncompletePreview = percent
                            
                            Sane.runOn(mainThread: true) {
                                // image creation needs to be done on main thread
                                let incompleteImage = try? UIImage.sy_imageFromIncompleteSane(data: data, parameters: parameters)
                                progress(percent, incompleteImage)
                            }
                        }
                    }
                    else {
                        Sane.runOn(mainThread: true) {
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
                Sane.runOn(mainThread: mainThread) { completion?(nil, nil, SaneError.fromStatus(status)) }
                return
            }

            do {
                let image = try UIImage.sy_imageFromSane(source: UIImage.SaneSource.data(data), parameters: parameters!)
                Sane.runOn(mainThread: mainThread) { completion?(image, parameters, nil) }
            }
            catch {
                Sane.runOn(mainThread: mainThread) { completion?(nil, nil, error) }
            }
        }
    }
    
    public func cancelCurrentScan() {
        self.stopScanOperation = true
    }
}
