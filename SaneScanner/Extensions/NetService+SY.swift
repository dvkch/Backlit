//
//  NetService+SY.swift
//  SaneScanner
//
//  Created by syan on 24/02/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

extension NetService {
    func ipAddresses(allowIPv6: Bool) -> [(String, Int)]? {
        return addresses?.compactMap { $0.ipAddress(allowIPv6: allowIPv6) }
    }
}

private extension Data {
    func ipAddress(allowIPv6: Bool) -> (String, Int)? {
        var hostBuffer = [CChar](repeating: 0, count: Int(NI_MAXHOST))
        var portBuffer = [CChar](repeating: 0, count: Int(NI_MAXSERV))

        let err = withUnsafeBytes { buf -> Int32 in
            let sa = buf.baseAddress!.assumingMemoryBound(to: sockaddr.self)

            if sa.pointee.sa_family == AF_INET6 && !allowIPv6 {
                return -1
            }

            return getnameinfo(
                sa, socklen_t(sa.pointee.sa_len),
                &hostBuffer, socklen_t(hostBuffer.count),
                &portBuffer, socklen_t(portBuffer.count),
                NI_NUMERICHOST | NI_NUMERICSERV
            )
        }

        guard err == 0 else { return nil }
        
        let host = String(cString: hostBuffer)
        guard let port = Int(String(cString: portBuffer)) else { return nil }
        return (host, port)
    }
}
