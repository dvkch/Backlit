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
    func saneDidUpdateConfig(_ sane: Sane, previousConfig: SaneConfig)
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
        
        SaneConfig.makeConfigAvailableToSaneLib()
        SaneConfig.persist(configuration)
    }
    
    // MARK: Properties
    public weak var delegate: SaneDelegate?
    public private(set) var saneInitError: String?
    public var isUpdatingDevices: Bool {
        return runningDeviceUpdates > 0
    }
    public private(set) var saneVersion: OperatingSystemVersion = .init()

    // MARK: Private properties
    private var thread: Thread!
    private var saneStarted = false
    @SaneLocked private var runningDeviceUpdates: Int = 0 {
        didSet {
            guard runningDeviceUpdates != oldValue else { return }

            DispatchQueue.main.async {
                if self.isUpdatingDevices {
                    self.delegate?.saneDidStartUpdatingDevices(self)
                } else {
                    self.delegate?.saneDidEndUpdatingDevices(self)
                }
            }
        }
    }
    @SaneLocked private var openedDevices = [Device.Name: Device.Handle]()
    private var stopScanOperation = false

    // MARK: Configuration
    public var configuration: SaneConfig = SaneConfig.restored() ?? SaneConfig() {
        didSet {
            guard configuration != oldValue else { return }
            SaneConfig.persist(configuration)
            delegate?.saneDidUpdateConfig(self, previousConfig: oldValue)
        }
    }
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
            if Thread.isMainThread {
                block()
            }
            else {
                // async could be useful, for instance to allow generation of previews on the main thread without
                // blocking the reading of more data, but this used to lead to out of order blocks, putting the UI
                // at of sync with the actual scanning status (e.g. receives notification for scanning progress,
                // then scanning end, then scanning progress again)
                DispatchQueue.main.sync {
                    block()
                }
            }
        }
        else {
            block()
        }
    }
}

// MARK: Benchmarking
extension Sane {
    private static func logTime<T>(ifOver minReportDuration: TimeInterval = 1, function: String = #function, message: String? = nil, block: () -> T) -> T {
        let startDate = Date()
        let returnValue = block()
        let interval = Date().timeIntervalSince(startDate)
        if interval > minReportDuration {
            if let message = message {
                SaneLogger.w(.sane, "\(function), \(message): \(interval)s")
            } else {
                SaneLogger.w(.sane, "\(function): \(interval)s")
            }
        }
        return returnValue
    }
}

// MARK: Sane start and stop
extension Sane {
    private func startSane() {
        runOnSaneThread {
            guard !self.saneStarted else { return }
            
            self.clearOpenedDevices()
            self.runningDeviceUpdates = 0
            
            var version: SANE_Int = 0
            let s = sane_init(&version, SaneAuthenticationCallback(deviceName:username:password:))
            self.saneVersion = SaneVersionFromInt(version)
            
            if s != SANE_STATUS_GOOD {
                self.saneInitError = s.description
            } else {
                self.saneInitError = nil
            }
            
            self.saneStarted = (s == SANE_STATUS_GOOD)
            SaneLogger.i(.sane, "Started: \(self.saneStarted)")
        }
    }
    
    private func stopSane() {
        runOnSaneThread {
            guard self.saneStarted && self.openedDevices.isEmpty else { return }
            
            self.clearOpenedDevices()
            self.runningDeviceUpdates = 0
            sane_exit()
            
            self.saneStarted = false
            SaneLogger.i(.sane, "Stopped")
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
        SaneLogger.i(.sane, "Asking auth for \(deviceString ?? "<null>")")
        Sane.shared.delegate?.saneNeedsAuth(Sane.shared, for: deviceString, completion: { (authentication) in
            auth = authentication
            SaneLogger.i(.sane, "Received auth")
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
    public func updateDevices(completion: @escaping SaneCompletion<[Device]>) {
        // if multiple updates are sent, we'll do them all, without sending "saneDidEndUpdatingDevices"
        // to the delegate in between
        self.runningDeviceUpdates += 1

        SaneLogger.i(.sane, "Updating devices")
        runOnSaneThread {
            self.startSane()
            defer {
                self.stopSane()
                self.runningDeviceUpdates -= 1
            }
            
            var rawDevices: UnsafeMutablePointer<UnsafePointer<SANE_Device>?>? = nil
            
            let s: SANE_Status = Sane.logTime {
                sane_get_devices(&rawDevices, SANE_FALSE)
            }
            
            guard s == SANE_STATUS_GOOD else {
                Sane.runOn(mainThread: true, block: {
                    SaneLogger.e(.sane, "Couldn't update devices: \(s)")
                    completion(.failure(SaneError(saneStatus: s)))
                })
                return
            }
            
            var devices = [Device]()
            while let rawDevice = rawDevices?.advanced(by: devices.count).pointee {
                devices.append(Device(cDevice: rawDevice.pointee))
            }

            Sane.runOn(mainThread: true, block: {
                SaneLogger.i(.sane, "Found \(devices.count) devices")
                completion(.success(devices))
            })
        }
    }
    
    public func openDevice(_ device: Device, listOptions: Bool = true, completion: @escaping SaneCompletion<()>) {
        SaneLogger.i(.sane, "Opening \(device.model)")
        let startedOnMainThread = Thread.isMainThread
        
        runOnSaneThread {
            guard self.isDeviceOpened(device) == false else {
                SaneLogger.w(.sane, "Device is already opened")
                Sane.runOn(mainThread: startedOnMainThread, block: {
                    completion(.success(()))
                })
                return
            }
            self.startSane()
            
            var h: SANE_Handle? = nil
            
            let s: SANE_Status = Sane.logTime {
                sane_open(device.name.rawValue.cString(using: .utf8), &h)
            }
            
            if s == SANE_STATUS_GOOD {
                self.markDevice(device: device, openedWith: h)
            }
            
            if s == SANE_STATUS_GOOD && listOptions {
                self.listOptions(for: device, completion: nil)
            }

            Sane.runOn(mainThread: startedOnMainThread, block: {
                if s != SANE_STATUS_GOOD {
                    SaneLogger.e(.sane, "Couldn't open device: \(s)")
                    completion(.failure(SaneError(saneStatus: s)))
                }
                else {
                    SaneLogger.i(.sane, "Device opened")
                    completion(.success(()))
                }
            })
        }
    }
    
    public func closeDevice(_ device: Device) {
        SaneLogger.i(.sane, "Closing device \(device.model)")
        device.currentOperation = nil
        device.updatePreviewImage(nil, fallbackToExisting: false)

        runOnSaneThread {
            guard let handle = self.obtainDeviceHandle(device: device) else {
                SaneLogger.w(.sane, "Device doesn't seem opened, skipping")
                return
            }
            Sane.logTime { sane_close(handle) }
            self.markDevice(device: device, openedWith: nil)
            SaneLogger.i(.sane, "Device closed")

            if self.openedDevices.isEmpty {
                self.stopSane()
            }
        }
    }
    
    private func clearOpenedDevices() {
        openedDevices = [:]
    }
    
    private func obtainDeviceHandle(device: Device) -> SANE_Handle? {
        return openedDevices[device.name]?.pointer
    }
    
    private func markDevice(device: Device, openedWith pointer: SANE_Handle?) {
        openedDevices[device.name] = .init(pointer: pointer)
    }
    
    public func isDeviceOpened(_ device: Device) -> Bool {
        return obtainDeviceHandle(device: device) != nil
    }
}

extension Sane {
    // MARK: Options
    public func listOptions(for device: Device, completion: SaneCompletion<()>?) {
        SaneLogger.i(.sane, "Listing options for \(device.model)")
        let startedOnMainThread = Thread.isMainThread
        
        guard let handle = obtainDeviceHandle(device: device) else {
            SaneLogger.e(.sane, "Device is not opened, skipping")
            completion?(.failure(.deviceNotOpened))
            return
        }

        runOnSaneThread {
            var options = [DeviceOption]()
            
            Sane.logTime {
                
                var count = SANE_Int(0)
                
                // needed for sane to update the value of the option count
                var descriptor = sane_get_option_descriptor(handle, 0)
                
                let s = sane_control_option(handle, 0, SANE_ACTION_GET_VALUE, &count, nil)
                guard s == SANE_STATUS_GOOD else {
                    SaneLogger.e(.sane, "Couldn't get the number of options, aborting with status \(s)")
                    Sane.runOn(mainThread: startedOnMainThread) { completion?(.failure(.couldntGetOptions)) }
                    return
                }
                
                SaneLogger.i(.sane, "Found \(count) options")
                for i in 1..<count {
                    descriptor = sane_get_option_descriptor(handle, i)
                    options.append(DeviceOption.typedOption(cOption: descriptor!.pointee, index: Int(i), device: device))
                }
                
                options.forEach { $0.refreshValue(nil) }
            }
            
            device.options = options
            
            SaneLogger.i(.sane, "Finished listing options")
            Sane.runOn(mainThread: startedOnMainThread) { completion?(.success(())) }
        }
    }
    
    public func valueForOption<V, T: DeviceOptionTyped<V>>(_ option: T, completion: @escaping SaneCompletion<V>) {
        let optionName = "\(option.device.model) > \(option.localizedTitle)"
        SaneLogger.d(.sane, "Get value for \(optionName)")
        let startedOnMainThread = Thread.isMainThread
        
        guard let handle = obtainDeviceHandle(device: option.device) else {
            SaneLogger.e(.sane, "> Device is not opened, aborting")
            completion(.failure(SaneError.deviceNotOpened))
            return
        }
        
        guard option.type != SANE_TYPE_GROUP else {
            SaneLogger.w(.sane, "> Group option, nothing to obtain, aborting")
            completion(.failure(SaneError.getValueForGroupType))
            return
        }
        
        guard option.type != SANE_TYPE_BUTTON else {
            SaneLogger.w(.sane, "> Button option, nothing to obtain, aborting")
            completion(.failure(SaneError.getValueForButtonType))
            return
        }
        
        runOnSaneThread {
            let bytes = malloc(option.size)!
            
            let s = Sane.logTime(message: option.localizedTitle) { sane_control_option(handle, SANE_Int(option.index), SANE_ACTION_GET_VALUE, bytes, nil) }
            let value = option.valueForBytes(bytes)
            free(bytes)

            // some backends allow reading the value fo a disabled option, so we try anyway, but ignore it if it fails
            guard s == SANE_STATUS_GOOD || !option.capabilities.isActive else {
                SaneLogger.e(.sane, "> Couldn't obtain value for \(optionName): \(s)")
                Sane.runOn(mainThread: startedOnMainThread) { completion(.failure(SaneError(saneStatus: s))) }
                return
            }
            
            SaneLogger.d(.sane, "> Obtained value: \(value), constraints: \(option.constraint.description)")
            Sane.runOn(mainThread: startedOnMainThread) { completion(.success(value)) }
        }
    }

    private func setCropArea(_ cropArea: CGRect, useAuto: Bool, device: Device, completion: SaneCompletion<()>?) {
        SaneLogger.i(.sane, "Setting crop area to \(useAuto ? "auto" : cropArea.debugDescription) for \(device.model)")
        let startedOnMainThread = Thread.isMainThread
        
        guard isDeviceOpened(device) else {
            SaneLogger.e(.sane, "Device is not opened")
            completion?(.failure(SaneError.deviceNotOpened))
            return
        }

        runOnSaneThread {
            var finalError: SaneError? = nil
            
            let stdOptions = [SaneStandardOption.areaTopLeftX, .areaTopLeftY, .areaBottomRightX, .areaBottomRightY]
            let values = [cropArea.minX, cropArea.minY, cropArea.maxX, cropArea.maxY]
            
            for (option, value) in zip(stdOptions, values) {
                SaneLogger.i(.sane, "Setting \(value) for \(option)")
                if let option = device.standardOption(for: option) as? DeviceOptionInt {
                    self.updateOption(option, with: .value(Int(value)), completion: { (result) in
                        finalError = result.error
                    })
                }
                if let option = device.standardOption(for: option) as? DeviceOptionFixed {
                    self.updateOption(option, with: .value(Double(value)), completion: { (result) in
                        finalError = result.error
                    })
                }

                guard finalError == nil else { break }
            }
            
            Sane.runOn(mainThread: startedOnMainThread) {
                if let finalError {
                    SaneLogger.e(.sane, "Finished setting crop area with error: \(finalError.localizedDescription)")
                    completion?(.failure(finalError))
                }
                else {
                    SaneLogger.i(.sane, "Finished setting crop area")
                    completion?(.success(()))
                }
            }
        }
    }

    public func updateOption<V, T: DeviceOptionTyped<V>>(_ option: T, with value: DeviceOptionNewValue<V>, completion: SaneCompletion<SaneInfo>?) {
        let startedOnMainThread = Thread.isMainThread
        SaneLogger.i(.sane, "Setting value \(value) for option \(option.localizedTitle)")

        guard let handle = obtainDeviceHandle(device: option.device) else {
            SaneLogger.e(.sane, "> Device is not opened")
            completion?(.failure(SaneError.deviceNotOpened))
            return
        }
        
        guard option.type != SANE_TYPE_GROUP else {
            SaneLogger.e(.sane, "> Cannot set value for a group option")
            completion?(.failure(SaneError.setValueForGroupType))
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
                byteValue?.assign(repeating: 0)
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
            
            let result: Result<SaneInfo, SaneError>
            if status == SANE_STATUS_GOOD {
                if SaneInfo(rawValue: info).contains(.reloadOptions) {
                    SaneLogger.d(.sane, "> Reloading all options after update")
                    // this is absolutely needed, because if the option declares it needs to reload other options, setting any option before
                    // doing so will result in SANE_STATUS_INVAL. So we make sure each changes that needs to reload options *does* reload them
                    self.listOptions(for: option.device, completion: nil)
                    SaneLogger.d(.sane, "> Reloaded all options after update")
                }
                else {
                    SaneLogger.d(.sane, "> Reloading option to make sure its value is correct")
                    // some changes can be accepted but inexact, or we used an auto value and need to figure out the value that is actually used
                    option.refreshValue(nil)
                    SaneLogger.d(.sane, "> Reloaded option after update")
                }
                result = .success(SaneInfo(rawValue: info))
            }
            else {
                SaneLogger.e(.sane, "> Couldn't update option \(option.localizedTitle): \(status)")
                result = .failure(SaneError(saneStatus: status))
            }
            
            Sane.runOn(mainThread: startedOnMainThread) { completion?(result) }
        }
    }
    
    internal typealias RestoreBlock = () -> ()
    internal func updateOptionForPreview<V, T: DeviceOptionTyped<V>>(_ option: T) -> RestoreBlock {
        guard Thread.current == self.thread else {
            fatalError("This method should only be used on the SANE thread")
        }
        
        let prevValue = option.value
        
        let restoreBlock = {
            SaneLogger.d(.sane, "Preview: Restoring option \(option.localizedTitle) to \(prevValue)")
            Sane.shared.updateOption(option, with: .value(prevValue), completion: nil)
        }
        
        switch option.bestPreviewValue {
        case .auto:
            SaneLogger.d(.sane, "Preview: Setting option \(option.localizedTitle) to auto")
            Sane.shared.updateOption(option, with: .auto, completion: nil)
            return restoreBlock
        case .value(let value):
            SaneLogger.d(.sane, "Preview: Setting option \(option.localizedTitle) to \(value)")
            Sane.shared.updateOption(option, with: .value(value), completion: nil)
            return restoreBlock
        case .none:
            SaneLogger.d(.sane, "Preview: Ignoring option \(option.localizedTitle)")
            return {}
        }
    }
}

extension Sane {
    // MARK: Scan
    public func preview(device: Device, progress: ((ScanProgress) -> ())?, completion: @escaping SaneCompletion<ScanImage>) {
        let startedOnMainThread = Thread.isMainThread
        SaneLogger.i(.sane, "Starting preview for \(device.model)")

        guard isDeviceOpened(device) else {
            SaneLogger.e(.sane, "> Device is not opened")
            completion(.failure(SaneError.deviceNotOpened))
            return
        }
        
        Sane.runOn(mainThread: true) { progress?(.warmingUp) }

        runOnSaneThread {
            SaneLogger.i(.sane, "Preview: preparing options")
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
            SaneLogger.i(.sane, "Preview: options prepared")

            var result: SaneResult<ScanImage>!

            self.internalScan(device: device, operation: .preview, useScanCropArea: false, generateIntermediateImages: true, progress: progress, completion: { r in
                result = r.map { $0.last! }
            })
            if let error = result.error {
                SaneLogger.e(.sane, "Preview: finished scanning with error: \(error)")
            }
            else {
                SaneLogger.i(.sane, "Preview: finished scanning")
            }

            SaneLogger.i(.sane, "Preview: restoring options")
            if let optionPreview = device.standardOption(for: .preview) as? DeviceOptionBool {
                self.updateOption(optionPreview, with: .value(false), completion: nil)
            }
            else {
                restoreBlocks.forEach { $0() }
            }
            SaneLogger.i(.sane, "Preview: restored options")

            SaneLogger.i(.sane, "Preview: updating preview image")
            device.updatePreviewImage(result.value, fallbackToExisting: false)

            SaneLogger.i(.sane, "Preview: finished")
            Sane.runOn(mainThread: startedOnMainThread) { completion(result!) }
        }
    }

    public func scan(device: Device, useScanCropArea: Bool = true, progress: ((ScanProgress) -> ())?, completion: @escaping SaneCompletion<[ScanImage]>) {
        internalScan(device: device, operation: .scan, useScanCropArea: useScanCropArea, generateIntermediateImages: false, progress: progress, completion: completion)
    }

    private func internalScan(device: Device, operation: ScanOperation, useScanCropArea: Bool, generateIntermediateImages: Bool, progress: ((ScanProgress) -> ())?, completion: @escaping SaneCompletion<[ScanImage]>) {
        SaneLogger.i(.sane, "Scan: starting scan for \(device.name)")

        let startedOnMainThread = Thread.isMainThread

        guard let handle = obtainDeviceHandle(device: device) else {
            SaneLogger.e(.sane, "> Device is not opened")
            Sane.runOn(mainThread: startedOnMainThread) { completion(.failure(SaneError.deviceNotOpened)) }
            return
        }
        
        Sane.runOn(mainThread: true) {
            device.currentOperation = (operation, .warmingUp)
            progress?(.warmingUp)
        }
        
        let fullCompletion = { (scans: SaneResult<[ScanImage]>) in
            device.currentOperation = nil
            completion(scans)
        }
        
        runOnSaneThread {
            self.stopScanOperation = false
            
            let crop: CGRect
            if device.canCrop && useScanCropArea {
                SaneLogger.i(.sane, "> Setting crop area")
                self.setCropArea(device.cropArea, useAuto: false, device: device, completion: nil)
                crop = device.cropArea
            }
            else {
                SaneLogger.i(.sane, "> Using max crop area")
                crop = device.maxCropArea
            }
            
            // LATER: this is absolutely not garanteed by SANE, might only work on the test device
            let currentSource = (device.standardOption(for: .source) as? DeviceOptionTyped<String>)?.value
            let usingFeeder = currentSource == "Automatic Document Feeder"

            var results = [SaneResult<ScanImage>]()
            let previousSaneStatus = { results.last?.error?.saneStatus ?? SANE_STATUS_GOOD }
            let scannedLastImage = {
                let errorOrOutOfDocs = previousSaneStatus() != SANE_STATUS_GOOD
                let lastImageAcquiredInFull = results.last?.value?.parameters.acquiringLastFrame == true
                let acquiredSingleImageInFull = (!usingFeeder || operation == .preview) && lastImageAcquiredInFull
                // some drivers (e.g. epsonscan2) report Out of documents even when "Flatbed" source is selected.
                // some do not (e.g. test), so we resort to assuming if we're using a document feeder or not and check if the
                // acquired frame is complete or not
                return errorOrOutOfDocs || acquiredSingleImageInFull
            }

            while results.last?.error == nil && !scannedLastImage() {
                let lastFullFrameIndex = results.lastIndex(where: { (try? $0.get())?.1.acquiringLastFrame == true }) ?? -1
                let previousFrames = results.enumerated().filter { $0.offset > lastFullFrameIndex }.map(\.element).map { try! $0.get() }

                let result = self.internalFrameScan(
                    device: device, handle: handle, crop: crop,
                    generateIntermediateImages: generateIntermediateImages,
                    scannedDocsCount: results.filter { $0.value?.parameters.acquiringLastFrame == true }.count,
                    previousFrames: previousFrames
                ) { p in
                    Sane.runOn(mainThread: true) { progress?(p) }
                }
                results.append(result)
            }
            
            // remove last error if empty ADF
            if results.count > 1, previousSaneStatus() == SANE_STATUS_NO_DOCS {
                results.removeLast()
            }
            
            // remove intermediary frames
            results = results
                .filter { $0.value?.parameters.acquiringLastFrame != false }
                .map { $0.map { scan in (scan.image, scan.parameters.singleFrameEquivalent) } }
            
            // after a finished scan, we need to call cancel, as per the documentation
            sane_cancel(handle)

            let scans = results.flattened()
            Sane.runOn(mainThread: startedOnMainThread) {
                if let lastScan = (try? scans.get())?.last {
                    device.updatePreviewImage(lastScan, fallbackToExisting: true)
                }
                fullCompletion(scans)
            }
        }
    }
    
    /// Device should be opened at this point
    private func internalFrameScan(device: Device, handle: SANE_Handle, crop: CGRect, generateIntermediateImages: Bool, scannedDocsCount: Int, previousFrames: [ScanImage], progress: @escaping (ScanProgress) -> ()) -> SaneResult<ScanImage> {

        SaneLogger.i(.sane, "> Starting scan")
        var status = Sane.logTime { sane_start(handle) }
        
        guard status == SANE_STATUS_GOOD else {
            SaneLogger.e(.sane, "> Couldn't start scan: \(status)")
            return .failure(SaneError(saneStatus: status))
        }
        
        SaneLogger.i(.sane, "> Setting blocking IO mode")
        status = sane_set_io_mode(handle, SANE_FALSE) // false = blocking IO mode
        
        guard status == SANE_STATUS_GOOD || status == SANE_STATUS_UNSUPPORTED else {
            SaneLogger.e(.sane, "> Couldn't set IO mode: \(status)")
            return .failure(SaneError(saneStatus: status))
        }

        SaneLogger.i(.sane, "> Obtaining scan parameters")
        var params = SANE_Parameters()
        status = sane_get_parameters(handle, &params)

        guard status == SANE_STATUS_GOOD else {
            SaneLogger.e(.sane, "> Couldn't obtain scan parameters: \(status)")
            return .failure(SaneError(saneStatus: status))
        }

        // LATER: handle lines == -1 when height isn't known at first
        // read scan parameters as soon as possible. now that the scan has started they should not change. this is
        // helpful since some backends don't give valid ones later on, especially when scanning in non blocking
        // IO mode
        let parameters = ScanParameters(cParams: params, cropArea: crop)
        assert(parameters.fileSize > 0, "Scan parameters invalid")
        SaneLogger.i(.sane, "> Scan parameters are \(parameters)")

        var data = Data(capacity: parameters.fileSize)
        if parameters.expectedFramesCount > 1 {
            let prevData = previousFrames.last?.image.cgImage?.dataProvider?.data as Data?
            data = prevData.map { Data($0) } ?? Data(repeating: 0, count: parameters.fileSize * parameters.expectedFramesCount)
        }

        let bufferMaxSize = max(500 * 1000, parameters.fileSize / 100)
        SaneLogger.d(.sane, "> Preparing buffer of size \(bufferMaxSize) bytes")

        let buffer = malloc(bufferMaxSize)!.bindMemory(to: UInt8.self, capacity: bufferMaxSize)
        defer {
            SaneLogger.d(.sane, "> Freeing buffer")
            free(buffer)
        }
        var bufferActualSize: SANE_Int = 0
        
        // generate incomplete image preview every 5%
        // LATER: compute that value dynamically depending on how long it took to generate the previous incomplete image
        var progressForLastIncompletePreview: Float = 0
        let incompletePreviewStep: Float = 0.05
        
        var acquiredBytesCount = 0
        while !stopScanOperation {
            // read data
            status = sane_read(handle, buffer, SANE_Int(bufferMaxSize), &bufferActualSize)
            guard status == SANE_STATUS_GOOD else { break }
            SaneLogger.d(.sane, "> Reading next data: requested \(bufferMaxSize), got \(bufferActualSize) bytes")

            // lineart requires inverting pixel values
            if parameters.depth == 1 && parameters.currentlyAcquiredFrame == SANE_FRAME_GRAY {
                (0..<Int(bufferActualSize)).forEach { i in
                    buffer[i] = ~buffer[i]
                }
            }
            
            // append data from the buffer to the total data
            if parameters.expectedFramesCount == 1 {
                // could buffer to a file instead [fileHandle writeData:[NSData dataWithBytes:buffer length:bufferActualSize]]
                SaneLogger.d(.sane, "> Appending image data")
                data.append(buffer, count: Int(bufferActualSize))
            }
            else {
                // LATER: support 1-bit color. image format dictates they will be interleaved (8 bits of red, then 8 bits
                // of green, then 8 bits of blue, representing conceptually 8 pixels of 1 bit per color). CGImage does handle this
                // properly, if we were able to construct that kind of image
                let bytesPerPixel = max(1, parameters.depth / 8)
                SaneLogger.d(.sane, "> Interlacing image data")
                (0..<(Int(bufferActualSize) / bytesPerPixel)).forEach { pixelIndex in
                    (0..<bytesPerPixel).forEach { byteIndex in
                        data[acquiredBytesCount * 3 + (pixelIndex * 3 + parameters.currentFrameIndex) * bytesPerPixel + byteIndex] = buffer[pixelIndex * bytesPerPixel + byteIndex]
                    }
                }
            }
            acquiredBytesCount += Int(bufferActualSize)

            // handle progress reporting
            let percentScanned = Float(acquiredBytesCount) / Float(parameters.fileSize)
            let globalPercentScanned = (percentScanned + Float(previousFrames.count)) / Float(parameters.expectedFramesCount)
            var imagePreviewData: Data? = nil
            var reportProgress = true

            if generateIntermediateImages {
                if percentScanned > progressForLastIncompletePreview + incompletePreviewStep {
                    progressForLastIncompletePreview = percentScanned
                    // copy the data before passing it to the main thread, or it might return size == 0 if it
                    // is being written to in the sane thread at the same time
                    imagePreviewData = Data(data)
                }
                else {
                    reportProgress = false
                }
            }
            if reportProgress {
                // image creation needs to be done on main thread
                Sane.runOn(mainThread: true) {
                    if imagePreviewData != nil {
                        SaneLogger.d(.sane, "> Generating preview: \(percentScanned)")
                    }
                    else {
                        SaneLogger.d(.sane, "> Reporting progress: \(percentScanned)")
                    }
                    let incompleteImage = imagePreviewData.map {
                        try? UIImage.sy_imageFromIncompleteSane(data: $0, parameters: parameters)
                    } ?? nil
                    let p: ScanProgress = .scanning(
                        progress: globalPercentScanned, finishedDocs: scannedDocsCount,
                        incompletePreview: incompleteImage, parameters: parameters
                    )
                    device.currentOperation?.progress = p
                    progress(p)
                }
            }
        }

        if self.stopScanOperation {
            SaneLogger.i(.sane, "> Scan cancel was required, stopping now")
            self.stopScanOperation = false
            Sane.runOn(mainThread: true) {
                device.currentOperation?.progress = .cancelling
                progress(.cancelling)
            }
            sane_cancel(handle)
            return .failure(SaneError.cancelled)
        }

        guard status == SANE_STATUS_EOF else {
            SaneLogger.e(.sane, "> Scan stopped unexpectedly: \(status)")
            return .failure(SaneError(saneStatus: status))
        }
        
        SaneLogger.i(.sane, "> Finished scanning \(parameters.currentlyAcquiredFrame) frame with success")
        let result = Result(catching: {
            try UIImage.sy_imageFromSane(data: data, parameters: parameters)
        })
        return result.map { ($0, parameters) }.mapError { $0 as! SaneError }
    }
    
    public func cancelCurrentScan() {
        self.stopScanOperation = true
    }
}
