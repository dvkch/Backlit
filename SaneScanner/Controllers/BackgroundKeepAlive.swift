//
//  BackgroundKeepAlive.swift
//  SaneScanner
//
//  Created by syan on 09/05/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit
import AVFoundation

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


/*
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
    private var identifier: UIBackgroundTaskIdentifier = .invalid
    private func updateStatus() {
        guard keepAlive else {
            Logger.i(.background, "Ending background task")
            UIApplication.shared.endBackgroundTask(identifier)
            return
        }

        identifier = UIApplication.shared.beginBackgroundTask(withName: "scan") {
            Logger.i(.background, "Ending background task")
            UIApplication.shared.endBackgroundTask(self.identifier)
            
            self.updateStatus()
        }
        
        if identifier == .invalid {
            Logger.e(.background, "Couldn't create background task")
        }
        else {
            Logger.i(.background, "Started background task")
        }
    }
}
*/
