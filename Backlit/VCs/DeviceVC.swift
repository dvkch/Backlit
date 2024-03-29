//
//  DeviceVC.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SYKit
import SYPictureMetadata
import StoreKit
import AVFAudio

protocol DeviceVCDelegate: NSObjectProtocol {
    func deviceVC(_ deviceVC: DeviceVC, didRefreshDevice device: Device)
}

class DeviceVC: UIViewController {

    init(device: Device) {
        self.device = device
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder aDecoder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .background
        title = device.model
        navigationItem.largeTitleDisplayMode = .never
        
        tableView.remembersLastFocusedIndexPath = true
        tableView.separatorStyle = UIDevice.isCatalyst ? .none : .singleLine
        tableView.clipsToBounds = true
        tableView.shouldDelayTouch = { view in
            // allow immediate highlight of pressed buttons, while allowing scroll when
            // panning from the ScanButton or anything else
            view.isKind(of: ScanButton.self) || !view.isKind(of: UIButton.self)
        }
        tableView.alwaysBounceVertical = true
        tableView.dataSource = self
        tableView.delegate = self
        if #available(macCatalyst 15.0, iOS 15.0, *) {
            tableView.isPrefetchingEnabled = false
        }
        tableView.contentInsetAdjustmentBehavior = .scrollableAxes
        tableView.registerHeader(TableViewHeader.self, xib: false)
        tableView.registerCell(PreviewCell.self, xib: true)
        tableView.registerCell(OptionCell.self, xib: true)
        view.addSubview(tableView)
        tableView.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }

        scanButtonStackView.distribution = .fill
        view.addSubview(scanButtonStackView)
        scanButtonStackView.snp.makeConstraints { make in
            make.left.right.equalToSuperview()
            make.bottom.equalTo(view.layoutMarginsGuide)
        }
        
        scanButton.kind = .scan
        scanButton.style = .cell
        scanButton.layer.masksToBounds = true
        scanButton.layer.cornerRadius = 10
        scanButton.layer.maskedCorners = [.layerMinXMinYCorner, .layerMaxXMinYCorner]
        scanButton.addTarget(self, action: #selector(scanButtonTap), for: .primaryActionTriggered)
        scanButtonStackView.addArrangedSubview(scanButton)

        belowScanButtonBackgroundView.backgroundColor = .tint
        view.addSubview(belowScanButtonBackgroundView)
        belowScanButtonBackgroundView.snp.makeConstraints { make in
            make.left.right.equalToSuperview()
            make.top.equalTo(scanButtonStackView.snp.bottom)
            make.bottom.equalToSuperview()
        }

        addKeyCommand(.preview)
        addKeyCommand(.scan)
        addKeyCommand(.abort)

        thumbsView = GalleryThumbsView.showInToolbar(of: self)
        
        if !UIDevice.isCatalyst {
            addKeyCommand(.settings)
            navigationItem.rightBarButtonItem = .settings(target: self, action: #selector(self.settingsButtonTap))
        }
        
        addKeyCommand(.refresh)
        refreshView = .init(tableView: tableView, viewController: self) { [weak self] in
            self?.refresh(userInitiated: true)
        }

        NotificationCenter.default.addObserver(self, selector: #selector(self.prefsChangedNotification), name: .preferencesChanged, object: nil)
        
        updateLayoutStyle()
        refresh(userInitiated: false)
    }
    
    deinit {
        if isScanning {
            SaneMockable.shared.cancelCurrentScan()
        }

        Sane.shared.closeDevice(device)
        NotificationCenter.default.removeObserver(self, name: .preferencesChanged, object: nil)
    }
    
    // MARK: Properties
    weak var delegate: DeviceVCDelegate?
    let device: Device
    var isScanning: Bool {
        SaneMockable.shared.scanSatus(for: device).isScanning
    }
    private(set) var isSavingScan: Bool = false
    var useLargeLayout: Bool = false {
        didSet {
            guard useLargeLayout != oldValue else { return }
            updateLayoutStyle()
        }
    }

    private var isRefreshing: Bool = false

    // MARK: Views
    private var thumbsView: GalleryThumbsView!
    private let tableView = TableView(frame: CGRect(x: 0, y: 0, width: 320, height: 600), style: .grouped)
    private let scanButtonStackView = UIStackView()
    private let belowScanButtonBackgroundView = UIView()
    private let scanButton = ScanButton(type: .custom)
    private var refreshView: RefreshView!
    
    // MARK: Actions
    func close(force: Bool = false, completion: (() -> ())? = nil) {
        guard !isScanning || force else  {
            showDismissalConfirmation {
                self.cancelOperation()
                self.close(force: true, completion: completion)
            }
            return
        }

        navigationController?.popViewController(animated: true) {
            completion?()
        }
    }
    
    override func canPerformAction(_ action: Selector, withSender sender: Any?) -> Bool {
        if action == #selector(scan) || action == #selector(preview) {
            return !isScanning
        }
        if action == #selector(cancelOperation) {
            return isScanning
        }
        return super.canPerformAction(action, withSender: sender)
    }
    
    @objc private func scanButtonTap() {
        if isScanning {
            SaneMockable.shared.cancelCurrentScan()
        }
        else {
            scan()
        }
    }

    @objc func scan() {
        guard !isScanning else { return }
        
        askForLocalNotificationsPermission()

        Analytics.shared.send(event: .scanStarted(device: device))

        // block that will be called to show progress
        let progressBlock = { [weak self] (status: Device.Status) -> () in
            self?.updateAfterScanStatusChange(status)
        }
        
        // block that will be called to handle completion
        let completionBlock = { [weak self] (results: SaneResult<[ScanImage]>) in
            guard let self = self else { return }

            switch results {
            case .success(let scans):
                self.isSavingScan = true
                Analytics.shared.send(event: .scanEnded(device: self.device, imagesCount: scans.count))
                GalleryManager.shared.saveScans(device: device, scans) { saveResult in
                    self.updateAfterScanStatusChange(nil)
                    self.isSavingScan = false

                    switch saveResult {
                    case .success(let items):
                        Analytics.shared.askPermission(from: self)
                        self.presentReviewPrompt()
                        self.presentFinishedScanNotification(items: items)

                    case .failure(let error):
                        UIAlertController.show(for: error, in: self)
                    }
                }

            case .failure(let error):
                self.updateAfterScanStatusChange(nil)
                if case .cancelled = error {
                    Analytics.shared.send(event: .scanCancelled(device: self.device))
                } else {
                    Analytics.shared.send(event: .scanFailed(device: self.device, error: error))
                }
                UIAlertController.show(for: error, in: self)
            }
        }

        // start scan
        SaneMockable.shared.scan(device: device, progress: progressBlock, completion: completionBlock)
    }

    @objc func preview() {
        guard !isScanning else { return }

        Analytics.shared.send(event: .previewStarted(device: device))

        SaneMockable.shared.preview(device: device, progress: { [weak self] (p) in
            self?.updateAfterScanStatusChange(p)
        }, completion: { [weak self] (result) in
            guard let self = self else { return }
            self.updateAfterScanStatusChange(nil)
            if case let .failure(error) = result {
                if case .cancelled = error {
                    Analytics.shared.send(event: .previewCancelled(device: self.device))
                } else {
                    Analytics.shared.send(event: .previewFailed(device: self.device, error: error))
                }
                UIAlertController.show(for: error, in: self)
            }
            else {
                Analytics.shared.send(event: .previewEnded(device: self.device))
            }
        })
    }
    
    @objc func cancelOperation() {
        SaneMockable.shared.cancelCurrentScan()
    }

    @objc private func settingsButtonTap() {
        let nc = UINavigationController(rootViewController: PreferencesVC())
        nc.modalPresentationStyle = .formSheet
        present(nc, animated: true, completion: nil)
    }
    
    @objc private func prefsChangedNotification() {
        tableView.reloadData()
    }
    
    private func presentReviewPrompt() {
        #if !DEBUG
        if #available(iOS 14.0, macCatalyst 14.0, *), let scene = view.window?.windowScene {
            SKStoreReviewController.requestReview(in: scene)
        }
        else {
            SKStoreReviewController.requestReview()
        }
        #endif
    }
    
    private func askForLocalNotificationsPermission() {
        #if !targetEnvironment(simulator)
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound]) { _, _ in () }
        #endif
    }
    
    private func presentFinishedScanNotification(items: [GalleryItem]) {
        guard UIApplication.shared.applicationState == .background, let firstItem = items.first else {
            let notifSound = Bundle.main.url(forResource: "scanner", withExtension: "caf")!
            let player = try! AVAudioPlayer(contentsOf: notifSound)
            player.play()
            DispatchQueue.main.asyncAfter(deadline: .now() + player.duration) {
                player.stop()
            }
            return
        }

        let content = UNMutableNotificationContent()
        content.title = "SCAN FINISHED NOTIFICATION TITLE".localized
        content.body = "SCAN FINISHED NOTIFICATION MESSAGE %d".localized(quantity: items.count)
        content.sound = .init(named: .init(rawValue: "scanner.caf"))
        content.attachments = items.compactMap {
            // https://stackoverflow.com/a/77157008/1439489
            if let url = try? ($0.lowResURL ?? $0.url).temporaryCopy() {
                return try? UNNotificationAttachment(identifier: $0.url.path, url: url)
            }
            return nil
        }
        content.userInfo = ["galleryItemPath": firstItem.url.path]
        let trigger = UNTimeIntervalNotificationTrigger(timeInterval: 1, repeats: false)
        let request = UNNotificationRequest(identifier: "scan-\(firstItem.hash)", content: content, trigger: trigger)
        UNUserNotificationCenter.current().add(request) { error in
            if let error {
                Logger.e(.app, "Couldn't show local notification for finished scan: \(error)")
            }
        }
    }
    
    // MARK: Content
    @objc private func refresh(userInitiated: Bool) {
        guard !isRefreshing else { return }
        isRefreshing = true
        
        refreshView.startLoading(discreet: !userInitiated)
        Sane.shared.listOptions(for: device) { [weak self] _ in
            guard let self = self else { return }
            self.tableView.reloadData()
            self.isRefreshing = false
            self.refreshView.stopLoading()
            self.delegate?.deviceVC(self, didRefreshDevice: self.device)
            
            DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
                self.prepareForSnapshotting()
            }
        }
    }
    
    var previewCell: PreviewCell? {
        return tableView.visibleCells.compactMap({ $0 as? PreviewCell }).first
    }
    
    private func updateAfterScanStatusChange(_ status: Device.Status) {
        switch status?.operation {
        case .scan:
            self.scanButton.progress = status?.progress
        case .preview:
            self.scanButton.isEnabled = status?.progress == nil
        case .none:
            self.scanButton.isEnabled = true
            self.scanButton.progress = nil
        }
        previewCell?.refresh()
        (splitViewController as? SplitVC)?.previewNC.viewControllers.forEach { ($0 as? DevicePreviewVC)?.refresh() }
        navigationController?.interactivePopGestureRecognizer?.isEnabled = !isScanning
    }
    
    func optionGroups() -> [DeviceOptionGroup] {
        return device.optionGroups(includeAdvanced: Preferences.shared.showAdvancedOptions)
    }
    
    func optionGroup(tableViewSection section: Int) -> DeviceOptionGroup? {
        guard section - 1 < optionGroups().count else { return nil }
        return optionGroups()[section - 1]
    }
    
    func optionsInGroup(tableViewSection section: Int) -> [DeviceOption]? {
        return optionGroup(tableViewSection: section)?.options(includeAdvanced: Preferences.shared.showAdvancedOptions)
    }

    // MARK: Layout
    private func updateLayoutStyle() {
        scanButton.sy_isHidden = useLargeLayout
        belowScanButtonBackgroundView.sy_isHidden = useLargeLayout
        tableView.reloadData()
    }
    
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        tableView.contentInset.bottom = scanButton.sy_isHidden ? 0 : scanButton.bounds.height
        tableView.horizontalScrollIndicatorInsets.bottom = tableView.contentInset.bottom
        tableView.verticalScrollIndicatorInsets.bottom = tableView.contentInset.bottom
    }
}

// MARK: Snapshots
extension DeviceVC {
    private func prepareForSnapshotting() {
        #if DEBUG
        Snapshot.setup { config in
            if config.kind == .deviceOptions || config.kind == .deviceOptionPopup {
                let firstOption = IndexPath(row: 0, section: 1)
                tableView.scrollToRow(at: firstOption, at: .top, animated: false)
            }
        
            if config.kind == .deviceOptionPopup {
                let firstOption = IndexPath(row: 0, section: 1)
                if let cell = tableView.cellForRow(at: firstOption) as? OptionCell, cell.hasControlToUpdateItsValue {
                    cell.triggerValueControl()
                }
                else {
                    self.tableView(tableView, didSelectRowAt: firstOption)
                }
            }
        }
        #endif
    }
}

// MARK: Dismissal
extension DeviceVC : ConditionallyDismissible {
    var isDismissible: Bool {
        return !isScanning
    }
    
    var dismissalConfirmationTexts: DismissalTexts {
        return .init(
            title: "CLOSE DEVICE CONFIRMATION TITLE".localized,
            message: "CLOSE DEVICE CONFIRMATION MESSAGE".localized,
            dismissButton: "ACTION STOP SCANNING".localized,
            cancelButton: "ACTION CONTINUE".localized
        )
    }
}

// MARK: Scan options
extension DeviceVC : UITableViewDataSource {
    func numberOfSections(in tableView: UITableView) -> Int {
        if device.options.isEmpty {
            return 0
        }
        return optionGroups().count + 1
    }
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        if section == 0 {
            return useLargeLayout ? 0 : 1
        }
        return optionsInGroup(tableViewSection: section)?.count ?? 0
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        if indexPath.section == 0 {
            let cell = tableView.dequeueCell(PreviewCell.self, for: indexPath)
            cell.device = device
            cell.delegate = self
            return cell
        }
        
        let cell = tableView.dequeueCell(OptionCell.self, for: indexPath)
        cell.delegate = self
        if let option = optionsInGroup(tableViewSection: indexPath.section)?[indexPath.row] {
            cell.updateWith(option: option)
        }
        cell.showDescription = false
        return cell
    }
    
    private func headerText(in section: Int) -> String? {
        if section == 0 && useLargeLayout {
            return nil
        }

        if section == 0 {
            return "DEVICE SECTION PREVIEW".localized
        }
        else {
            return optionGroup(tableViewSection: section)?.localizedTitle
        }
    }
    
    func tableView(_ tableView: UITableView, viewForHeaderInSection section: Int) -> UIView? {
        guard let text = headerText(in: section) else { return nil }

        let header = tableView.dequeueHeader(TableViewHeader.self)
        header.text = text
        return header
    }
    
    private func previewCellHeight(in tableView: UITableView) -> CGFloat {
        var maxImageHeight = tableView.bounds.height / 2
        
        if traitCollection.verticalSizeClass == .compact {
            maxImageHeight = 400
        }
        
        return PreviewCell.cellHeight(device: device, width: tableView.bounds.width, maxImageHeight: maxImageHeight)
    }
    
    func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        if indexPath.section == 0 {
            return previewCellHeight(in: tableView)
        }
        
        return UITableView.automaticDimension
    }
    
    func tableView(_ tableView: UITableView, estimatedHeightForRowAt indexPath: IndexPath) -> CGFloat {
        if indexPath.section == 0 {
            // estimation needs to be a realistic value, or else the tableView will jump each time it's reloaded
            return previewCellHeight(in: tableView)
        }
        
        if let option = optionsInGroup(tableViewSection: indexPath.section)?[indexPath.row] {
            return OptionCell.cellHeight(option: option, showDescription: false, width: tableView.bounds.width)
        }
        
        return 0
    }
    
    func tableView(_ tableView: UITableView, heightForHeaderInSection section: Int) -> CGFloat {
        guard let text = headerText(in: section) else {
            return 1 // returning 0 doesn't work for a grouped table view
        }

        // UITableView.automaticDimension causes jumps in the first reloadData after opening the VC, seems
        // linked to largeTitle in navBar (enabled in prev VC, disabled in this one)
        return TableViewHeader.height(for: text, topMargin: section == 0 ? 17 : 0, at: section, in: tableView)
    }
}

extension DeviceVC : UITableViewDelegate {
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        guard indexPath.section > 0, let option = optionsInGroup(tableViewSection: indexPath.section)?[indexPath.row] else { return }
        guard let cell = tableView.cellForRow(at: indexPath) as? OptionCell else { return }
        if UIDevice.isCatalyst && cell.hasControlToUpdateItsValue { return }

        let vc = DeviceOptionVC(option: option, showHelpOnly: cell.hasControlToUpdateItsValue, delegate: self)
        vc.popoverPresentationController?.sourceView = view
        if let cell = tableView.cellForRow(at: indexPath) {
            vc.popoverPresentationController?.sourceRect = view.convert(cell.bounds, from: cell)
        } else {
            vc.popoverPresentationController?.sourceRect = view.bounds
        }
        vc.popoverPresentationController?.permittedArrowDirections = [.left, .unknown]

        present(vc, animated: true, completion: nil)
    }
}

extension DeviceVC : PreviewViewDelegate {
    func previewView(_ previewView: PreviewView, device: Device, tapped action: ScanOperation) {
        guard !isScanning else { return }
        
        if action == .preview {
            preview()
        }
        if action == .scan {
            scan()
        }
    }
    
    func previewView(_ previewView: PreviewView, canceledScan device: Device) {
        SaneMockable.shared.cancelCurrentScan()
    }
}

extension DeviceVC : DeviceOptionControllableDelegate {
    func deviceOptionControllable(_ controllable: DeviceOptionControllable, willUpdate option: DeviceOption) {
        refreshView.startLoading(discreet: true)
    }
    
    func deviceOptionControllable(_ controllable: DeviceOptionControllable, didUpdate option: DeviceOption, result: Result<SaneInfo, SaneError>) {
        refreshView.stopLoading()

        switch result {
        case .success(let info):
            if info.contains(.reloadOptions) {
                tableView.reloadData()
                tableView.updateFocusIfNeeded()
            }
            else {
                tableView.visibleCells
                    .compactMap { $0 as? OptionCell }
                    .filter { $0.option == option }
                    .forEach { $0.updateWith(option: option) }
            }
            
        case .failure(let error):
            UIAlertController.show(for: error, in: self)
        }
    }
}
