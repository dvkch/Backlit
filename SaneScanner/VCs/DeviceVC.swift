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

        #if targetEnvironment(macCatalyst)
        tableView.separatorStyle = .none
        #endif
        tableView.clipsToBounds = true
        tableView.alwaysBounceVertical = true
        tableView.dataSource = self
        tableView.delegate = self
        tableView.registerHeader(TableViewHeader.self, xib: false)
        tableView.registerCell(PreviewCell.self, xib: true)
        tableView.registerCell(OptionCell.self, xib: true)
        view.addSubview(tableView)
        tableView.snp.makeConstraints { make in
            make.top.equalTo(view.layoutMarginsGuide)
            make.left.right.equalToSuperview()
        }

        scanButtonStackView.distribution = .fill
        view.addSubview(scanButtonStackView)
        scanButtonStackView.snp.makeConstraints { make in
            make.top.equalTo(tableView.snp.bottom)
            make.left.right.equalToSuperview()
            make.bottom.equalTo(view.layoutMarginsGuide)
        }
        
        scanButton.kind = .scan
        scanButton.style = .cell
        scanButtonStackView.addArrangedSubview(scanButton)
        
        thumbsView = GalleryThumbsView.showInToolbar(of: self, tintColor: .tint)
        
        navigationItem.rightBarButtonItem = PreferencesVC.settingsBarButtonItem(target: self, action: #selector(self.settingsButtonTap))
        
        tableView.addPullToResfresh { [weak self] (_) in
            self?.refresh()
        }
        
        NotificationCenter.default.addObserver(self, selector: #selector(self.prefsChangedNotification), name: .preferencesChanged, object: nil)
        
        updateLayoutStyle()
        refresh()
    }

    deinit {
        if isScanning {
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
    private var scanProgress: ScanProgress? = nil
    private var isScanning: Bool {
        switch scanProgress {
        case .warmingUp, .scanning: return true
        case .none, .cancelling: return false
        }
    }

    // MARK: Views
    private var thumbsView: GalleryThumbsView!
    private let tableView = UITableView(frame: .zero, style: .grouped)
    private let scanButtonStackView = UIStackView()
    private let scanButton = ScanButton(type: .custom)
    
    // MARK: Actions
    @IBAction private func scanButtonTap() {
        scan(device: device, progress: nil, completion: nil)
    }
    
    private func scan(device: Device, progress progressCallback: ((ScanProgress) -> ())?, completion: ((ScanResult) -> ())?) {
        if isScanning {
            Sane.shared.cancelCurrentScan()
            return
        }
        
        var progressVC: DeviceScanPreviewVC?
        var item: GalleryItem?
        
        // block that will be called to show progress
        let progressBlock = { [weak self] (progress: ScanProgress) in
            guard let self = self else { return }
            self.scanProgress = progress
            self.scanButton.progress = progress

            switch progress {
            case .warmingUp, .cancelling:
                break;

            case .scanning(_, let incompletePreview):
                progressVC?.isScanning = true
                progressVC?.image = incompletePreview ?? progressVC?.image
            }
            
            progressCallback?(progress)
        }
        
        // block that will be called to handle completion
        let completionBlock = { [weak self] (result: ScanResult) in
            guard let self = self else { return }
            self.scanProgress = nil
            self.scanButton.progress = nil

            switch result {
            case .success((let image, let parameters)):
                progressVC?.isScanning = false
                progressVC?.image = image

                let metadata = SYMetadata(device: self.device, scanParameters: parameters)
                do {
                    item = try GalleryManager.shared.addImage(image, metadata: metadata)
                }
                catch {
                    UIAlertController.show(for: error, in: self)
                }
                self.updatePreviewImageCell(image: image, scanParameters: parameters)

            case .failure(let error):
                progressVC?.dismiss(animated: false, completion: nil)
                UIAlertController.show(for: error, in: self)
            }
            
            completion?(result)
        }
        
        // need to show image (finished or partial with preview)
        #if !targetEnvironment(macCatalyst)
        if Sane.shared.configuration.showIncompleteScanImages {
            progressVC = DeviceScanPreviewVC(shareTap: {
                self.shareItem(item)
            }, cancelTap: {
                // cancels scan if running
                if self.isScanning {
                    Sane.shared.cancelCurrentScan()
                }
            })
            self.present(progressVC!, animated: true, completion: nil)
        }
        #endif

        // start scan
        Sane.shared.scan(device: device, progress: progressBlock, completion: completionBlock)
    }

    private func preview(device: Device, progress: ((ScanProgress) -> ())?, completion: ((ScanResult) -> ())?) {
        Sane.shared.preview(device: device, progress: { [weak self] (p) in
            self?.scanProgress = p
            progress?(p)
        }, completion: { [weak self] (result) in
            guard let self = self else { return }
            self.scanProgress = nil
            completion?(result)
            if case let .failure(error) = result {
                UIAlertController.show(for: error, in: self)
            }
        })
    }

    @objc private func settingsButtonTap() {
        let nc = UINavigationController(rootViewController: PreferencesVC())
        nc.modalPresentationStyle = .formSheet
        present(nc, animated: true, completion: nil)
    }
    
    #if !targetEnvironment(macCatalyst)
    private func shareItem(_ item: GalleryItem?) {
        guard let url = item?.URL else { return }
        UIActivityViewController.showForURLs([url], fromBottomIn: self, completion: nil)
    }
    #endif
    
    @objc private func prefsChangedNotification() {
        tableView.reloadData()
    }
    
    // MARK: Content
    private func refresh() {
        guard !isRefreshing else { return }
        isRefreshing = true
        
        tableView.showPullToRefresh()
        Sane.shared.listOptions(for: device) { [weak self] in
            guard let self = self else { return }
            self.tableView.reloadData()
            self.isRefreshing = false
            self.tableView.endPullToRefresh()
            self.delegate?.deviceVC(self, didRefreshDevice: self.device)
            
            DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
                self.prepareForSnapshotting()
            }
        }
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

    private func updatePreviewCell(cropAreaPercent: CGRect) {
        guard device.canCrop else { return }
        
        var cropArea = CGRect()
        cropArea.origin.x = device.maxCropArea.origin.x + device.maxCropArea.width * cropAreaPercent.origin.x
        cropArea.origin.y = device.maxCropArea.origin.y + device.maxCropArea.height * cropAreaPercent.origin.y
        cropArea.size.width = device.maxCropArea.width * cropAreaPercent.width
        cropArea.size.height = device.maxCropArea.height * cropAreaPercent.height

        device.cropArea = device.maxCropArea.intersection(cropArea)
        
        if let previewCell = tableView.visibleCells.compactMap({ $0 as? PreviewCell }).first {
            previewCell.refresh()
        }
    }
    
    private func updatePreviewImageCell(image: UIImage?, scanParameters: ScanParameters?) {
        // update only if we scanned without cropping
        guard device.cropArea == device.maxCropArea else { return }
        
        // prevent keeping a scan image if resolution is very high. A color A4 150dpi (6.7MB) is used as maximum
        guard scanParameters == nil || (scanParameters?.fileSize ?? 0) < 8_000_000 else { return }
    
        // if we require color mode to be set to auto, update only if auto is not available or scan mode is color
        let shouldUpdate: Bool
        if Preferences.shared.previewWithAutoColorMode, let colorOption = device.standardOption(for: .colorMode) as? DeviceOptionString {
            shouldUpdate = !colorOption.capabilities.contains(.automatic) || colorOption.value == SaneValueScanMode.color.value
        } else {
            shouldUpdate = true
        }
        
        guard shouldUpdate else { return }
    
        device.lastPreviewImage = image
        
        if let previewCell = tableView.visibleCells.compactMap({ $0 as? PreviewCell }).first {
            previewCell.refresh()
        }
    }

    // MARK: Layout
    private func updateLayoutStyle() {
        scanButton.sy_isHidden = useLargeLayout
        tableView.reloadData()
    }
}

// MARK: Snapshots
extension DeviceVC {
    private func prepareForSnapshotting() {
        let snapshotType = SnapshotKind.fromLaunchOptions
        guard snapshotType != .none else { return }
    
        if snapshotType == .devicePreview || snapshotType == .deviceOptions || snapshotType == .deviceOptionPopup {
            let rect = CGRect(x: 0.1, y: 0.2, width: 0.8, height: 0.6)
            if let path = SnapshotKind.snapshotTestScanImagePath {
                device.lastPreviewImage = UIImage(contentsOfFile: path)
            }
            updatePreviewCell(cropAreaPercent: rect)
        }

        if snapshotType == .deviceOptions || snapshotType == .deviceOptionPopup {
            let firstOption = IndexPath(row: 0, section: 1)
            tableView.scrollToRow(at: firstOption, at: .top, animated: false)
        }
    
        if snapshotType == .deviceOptionPopup {
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
        guard !cell.hasControlToUpdateItsValue else { return }

        let vc = DeviceOptionVC(option: option, delegate: self)
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

extension DeviceVC : SanePreviewViewDelegate {
    func sanePreviewView(_ sanePreviewView: SanePreviewView, device: Device, tapped action: SanePreviewView.Action, progress: ((ScanProgress) -> ())?, completion: ((ScanResult) -> ())?) {
        guard !isScanning else { return }
        
        if action == .preview {
            preview(device: device, progress: progress, completion: completion)
        }
        if action == .scan {
            scan(device: device, progress: progress, completion: completion)
        }
    }
    
    func sanePreviewView(_ sanePreviewView: SanePreviewView, canceledScan device: Device) {
        Sane.shared.cancelCurrentScan()
    }
}

extension DeviceVC : DeviceOptionControllableDelegate {
    func deviceOptionControllable(_ controllable: DeviceOptionControllable, willUpdate option: DeviceOption) {
        // TODO: show something and block next updates ?
    }
    func deviceOptionControllable(_ controllable: DeviceOptionControllable, didUpdate option: DeviceOption, error: Error?) {
        // TODO: hide loader if we presented any

        if let error = error {
            UIAlertController.show(for: error, in: self)
        }
        
        tableView.reloadData()
    }
}
