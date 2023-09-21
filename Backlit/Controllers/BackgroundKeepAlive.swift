//
//  BackgroundKeepAlive.swift
//  Backlit
//
//  Created by syan on 09/05/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import AVFoundation

// Nota: on older iOS version, this doesn't appear to work while debugging. After carefull testing without being attached
// to the debugger, it does indeed work. Tested on iOS 13.7
class BackgroundKeepAlive: NSObject {
    
    // MARK: Init
    static let shared = BackgroundKeepAlive()
    private override init() {
        super.init()
    }
    
    // MARK: Properties
    var keepAlive: Bool = false {
        didSet {
            guard keepAlive != oldValue else { return }
            Logger.i(.background, keepAlive ? "Enabling keepAlive" : "Disabling keepAlive")
            updateStatus()
        }
    }

    // MARK: Inner workings
    private let silentAudioURL = Bundle.main.url(forResource: "silent", withExtension: "mp3")!
    private var player = AVQueuePlayer()
    private var looper: AVPlayerLooper?
    private func updateStatus() {
        guard keepAlive else {
            try? AVAudioSession.sharedInstance().setActive(false, options: .notifyOthersOnDeactivation)
            looper?.disableLooping()
            looper = nil
            player.pause()
            return
        }

        try? AVAudioSession.sharedInstance().setCategory(.playback, options: .mixWithOthers)
        try? AVAudioSession.sharedInstance().setActive(true)

        looper = .init(player: player, templateItem: AVPlayerItem(url: silentAudioURL))
        player.play()
    }
}
