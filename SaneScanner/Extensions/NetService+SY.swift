//
//  NetService+SY.swift
//  SaneScanner
//
//  Created by syan on 24/02/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

extension NetService {
    enum AddressType {
        case ip4
        case ip6
    }
    func parsedAddresses(types: [AddressType]) -> [(ip: String, port: Int)]? {
        return addresses?.compactMap { $0.ipAddress(types: types) }
    }
}

private extension Data {
    func ipAddress(types: [NetService.AddressType]) -> (ip: String, port: Int)? {
        var hostBuffer = [CChar](repeating: 0, count: Int(NI_MAXHOST))
        var servBuffer = [CChar](repeating: 0, count: Int(NI_MAXSERV))

        let err = withUnsafeBytes { buf -> Int32 in
            let sa = buf.baseAddress!.assumingMemoryBound(to: sockaddr.self)

            if sa.pointee.sa_family == AF_INET && !types.contains(.ip4) {
                return -1
            }

            if sa.pointee.sa_family == AF_INET6 && !types.contains(.ip6) {
                return -1
            }

            return getnameinfo(
                sa, socklen_t(sa.pointee.sa_len),
                &hostBuffer, socklen_t(hostBuffer.count),
                &servBuffer, socklen_t(servBuffer.count),
                NI_NUMERICHOST | NI_NUMERICSERV
            )
        }
        guard err == 0 else { return nil }

        let host = String(cString: hostBuffer)
        guard let port = Int(String(cString: servBuffer)) else { return nil }
        return (host, port)
    }
}
