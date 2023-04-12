//
//  UIDevice+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 27/06/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit

public extension UIDevice {
    @objc(sy_isCatalyst)
    static var isCatalyst: Bool {
        #if targetEnvironment(macCatalyst)
        return true
        #else
        return false
        #endif
    }
    
    @objc(sy_usedMemory)
    var usedMemory: UInt {
        var info = task_basic_info()
        var size = mach_msg_type_number_t(MemoryLayout.size(ofValue: info))
        
        let kerr: kern_return_t = withUnsafeMutablePointer(to: &info) { pointer in
            return pointer.withMemoryRebound(to: integer_t.self, capacity: Int(size), { (rawPointer: UnsafeMutablePointer<integer_t>) in
                return task_info(mach_task_self_, task_flavor_t(TASK_BASIC_INFO), rawPointer, &size)
            })
        }
        
        if kerr != KERN_SUCCESS {
            let error = String(cString: mach_error_string(kerr))
            print("Error while obtaining used memory:", error)
        }
        
        return (kerr == KERN_SUCCESS) ? UInt(info.resident_size) : 0; // size in bytes
    }
    
    @objc(sy_freeMemory)
    var freeMemory: UInt {
        let host_port: mach_port_t = mach_host_self()
        var host_size = mach_msg_type_number_t(MemoryLayout.size(ofValue: vm_statistics_data_t()) / MemoryLayout.size(ofValue: integer_t()))
        
        var pageSize = vm_size_t(0)
        var vm_stat = vm_statistics_data_t()
        
        host_page_size(host_port, &pageSize)
        
        _ = withUnsafeMutablePointer(to: &vm_stat) { pointer in
            pointer.withMemoryRebound(to: integer_t.self, capacity: Int(host_size), { (rawPointer: UnsafeMutablePointer<integer_t>) in
                host_statistics(host_port, HOST_VM_INFO, rawPointer, &host_size)
            })
        }
        
        return UInt(vm_stat.free_count) * UInt(pageSize)
    }
    
    @objc(sy_logMemoryUsage)
    func logMemoryUsage() {
        let used = String(format: "Memory used: %7d kb", usedMemory / 1000)
        let free = String(format: "Free memory: %7d kb", freeMemory / 1000)
        print(used)
        print(free)
    }
}
