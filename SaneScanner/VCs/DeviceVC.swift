//
//  DeviceVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 04/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SYKit
import SYPictureMetadata
import StoreKit

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
        tableView.registerHeader(TableViewHeader.self, xib: false)
        tableView.registerCell(PreviewCell.self, xib: true)
        tableView.registerCell(OptionCell.self, xib: true)
        view.addSubview(tableView)
        tableView.snp.makeConstraints { make in
            make.top.bottom.equalTo(view.layoutMarginsGuide)
            make.left.right.equalToSuperview()
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

        let belowScanButtonBackgroundView = UIView()
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
            navigationItem.rightBarButtonItem = PreferencesVC.settingsBarButtonItem(target: self, action: #selector(self.settingsButtonTap))
        }
        
        addKeyCommand(.refresh)
        refreshView = .init(tableView: tableView, viewController: self) { [weak self] in
            self?.refresh()
        }

        NotificationCenter.default.addObserver(self, selector: #selector(self.prefsChangedNotification), name: .preferencesChanged, object: nil)
        
        updateLayoutStyle()
        refresh()
    }
    
    deinit {
        if device.isScanning {
            Sane.shared.cancelCurrentScan()
        }

        Sane.shared.closeDevice(device)
        NotificationCenter.default.removeObserver(self, name: .preferencesChanged, object: nil)
    }
    
    // MARK: Properties
    weak var delegate: DeviceVCDelegate?
    let device: Device
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
    private let scanButton = ScanButton(type: .custom)
    private var refreshView: RefreshView!
    
    // MARK: Actions
    override func canPerformAction(_ action: Selector, withSender sender: Any?) -> Bool {
        if action == #selector(scan) || action == #selector(preview) {
            return !device.isScanning
        }
        if action == #selector(cancelOperation) {
            return device.isScanning
        }
        return super.canPerformAction(action, withSender: sender)
    }
    
    @objc private func scanButtonTap() {
        if device.isScanning {
            Sane.shared.cancelCurrentScan()
        }
        else {
            scan()
        }
    }

    @objc func scan() {
        guard !device.isScanning else { return }

        Analytics.shared.send(event: .scanStarted(device: device))

        // block that will be called to show progress
        let progressBlock = { [weak self] (progress: ScanProgress) in
            guard let self = self else { return }
            self.scanButton.progress = progress
            self.updatePreviewViews()
        }
        
        // block that will be called to handle completion
        let completionBlock = { [weak self] (results: SaneResult<[ScanImage]>) in
            guard let self = self else { return }
            self.scanButton.progress = nil
            self.updatePreviewViews()

            switch results {
            case .success(let scans):
                Analytics.shared.send(event: .scanEnded(device: self.device, imagesCount: scans.count))
                do {
                    try GalleryManager.shared.saveScans(device: self.device, scans)
                    Analytics.shared.askPermission(from: self)
                    self.presentReviewPrompt()
                }
                catch {
                    UIAlertController.show(for: error, in: self)
                }

            case .failure(let error):
                if case .cancelled = error {
                    Analytics.shared.send(event: .scanCancelled(device: self.device))
                } else {
                    Analytics.shared.send(event: .scanFailed(device: self.device, error: error))
                }
                UIAlertController.show(for: error, in: self)
            }
        }

        // start scan
        Sane.shared.scan(device: device, progress: progressBlock, completion: completionBlock)
    }

    @objc func preview() {
        guard !device.isScanning else { return }

        Analytics.shared.send(event: .previewStarted(device: device))

        Sane.shared.preview(device: device, progress: { [weak self] (p) in
            self?.scanButton.isEnabled = false
            self?.updatePreviewViews()
        }, completion: { [weak self] (result) in
            guard let self = self else { return }
            self.scanButton.isEnabled = true
            self.updatePreviewViews()
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
        Sane.shared.cancelCurrentScan()
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
    
    // MARK: Content
    @objc private func refresh() {
        guard !isRefreshing else { return }
        isRefreshing = true
        
        refreshView.startLoading()
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
    
    private func updatePreviewViews() {
        tableView.visibleCells.compactMap({ $0 as? PreviewCell }).first?.refresh()
        (splitViewController as? SplitVC)?.previewNC.viewControllers.forEach { ($0 as? DevicePreviewVC)?.refresh() }
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
        guard Snapshot.isSnapshotting else { return }
    
        if Snapshot.kind == .deviceOptions || Snapshot.kind == .deviceOptionPopup {
            let firstOption = IndexPath(row: 0, section: 1)
            tableView.scrollToRow(at: firstOption, at: .top, animated: false)
        }
    
        if Snapshot.kind == .deviceOptionPopup {
            let firstOption = IndexPath(row: 0, section: 1)
            self.tableView(tableView, didSelectRowAt: firstOption)
        }
    }
}

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
    
    func tableView(_ tableView: UITableView, viewForHeaderInSection section: Int) -> UIView? {
        if section == 0 && useLargeLayout {
            return nil
        }

        let header = tableView.dequeueHeader(TableViewHeader.self)
        if section == 0 {
            header.text = "DEVICE SECTION PREVIEW".localized
        }
        else {
            header.text = optionGroup(tableViewSection: section)?.localizedTitle
        }
        return header
    }
    
    func previewCellHeight(in tableView: UITableView) -> CGFloat {
        var maxImageHeight = tableView.bounds.height * 2 / 3
        
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
        if section == 0 && useLargeLayout {
            return 1 // returning 0 doesn't work for a grouped table view
        }
        return UITableView.automaticDimension
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
        guard !device.isScanning else { return }
        
        if action == .preview {
            preview()
        }
        if action == .scan {
            scan()
        }
    }
    
    func previewView(_ previewView: PreviewView, canceledScan device: Device) {
        Sane.shared.cancelCurrentScan()
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
