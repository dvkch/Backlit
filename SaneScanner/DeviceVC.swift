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

#if !targetEnvironment(macCatalyst)
import SVProgressHUD
#endif

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

        tableView.registerCell(PreviewCell.self, xib: true)
        tableView.registerCell(OptionCell.self, xib: true)

        scanButton.backgroundColor = .tint
        scanButton.setTitle("ACTION SCAN".localized, for: .normal)
        scanButton.titleLabel?.font = .systemFont(ofSize: 17)
        scanButton.titleLabel?.autoAdjustsFontSize = true
        
        thumbsView = GalleryThumbsView.showInToolbar(of: self, tintColor: .tint)
        
        navigationItem.rightBarButtonItem = PreferencesVC.settingsBarButtonItem(target: self, action: #selector(self.settingsButtonTap))
        
        tableView.addPullToResfresh { [weak self] (_) in
            self?.refresh()
        }
        
        NotificationCenter.default.addObserver(self, selector: #selector(self.prefsChangedNotification), name: .preferencesChanged, object: nil)
        
        refresh()
    }

    deinit {
        Sane.shared.cancelCurrentScan()
        Sane.shared.closeDevice(device)
        NotificationCenter.default.removeObserver(self, name: .preferencesChanged, object: nil)
    }
    
    // MARK: Properties
    private let device: Device
    private var isRefreshing: Bool = false
    
    // MARK: Views
    private var thumbsView: GalleryThumbsView!
    @IBOutlet private var tableView: UITableView!
    @IBOutlet private var scanButton: UIButton!
    
    // MARK: Actions
    @IBAction private func scanButtonTap() {
        
        var alertView: UIAlertController?
        var alertViewCancelButton: UIAlertAction?
        var alertViewImageView: UIImageView?
        var item: GalleryItem?
        
        let block = { [weak self] (progress: Float, finished: Bool, image: UIImage?, parameters: ScanParameters?, error: Error?) in
            guard let self = self else { return }
            
            // Finished with error
            if let error = error {
                alertView?.dismiss(animated: false, completion: nil)
                SVProgressHUD.showError(withStatus: error.localizedDescription)
                return
            }
            
            // Finished without error
            if finished {
                alertViewCancelButton?.updateTitle("ACTION CLOSE".localized)
                
                guard let image = image, let parameters = parameters else { return }
                
                let metadata = SYMetadata(device: self.device, scanParameters: parameters)
                do {
                    item = try GalleryManager.shared.addImage(image, metadata: metadata)
                    SVProgressHUD.dismiss()
                }
                catch {
                    SVProgressHUD.showError(withStatus: error.localizedDescription)
                }
                self.updatePreviewImageCell(image: image, scanParameters: parameters)
            }
            
            // need to show image (finished or partial with preview)
            if alertView == nil, let image = image {
                alertView = UIAlertController(title: "DIALOG TITLE SCANNED IMAGE".localized, message: nil, preferredStyle: .alert)
                alertView?.addAction(UIAlertAction(title: "ACTION SHARE".localized, style: .default, handler: { (_) in
                    self.shareItem(item)
                }))
                alertViewCancelButton = UIAlertAction(title: "ACTION CANCEL".localized, style: .cancel, handler: { (_) in
                    // cancels scan if running
                    Sane.shared.cancelCurrentScan()
                })
                alertView?.addAction(alertViewCancelButton!)
                alertViewImageView = alertView?.setupImageView(image: image, height: 300, margins: UIEdgeInsets(top: 0, left: 0, bottom: 20, right: 0))
                self.present(alertView!, animated: true, completion: nil)
                SVProgressHUD.dismiss()
            }
            
            // update alertview
            alertView?.actions.forEach { $0.isEnabled = finished || $0.style == .cancel }
            
            // update image for partial preview
            if image != nil {
                alertViewImageView?.image = image
            }
            
            // update progress when no partial preview
            if !finished && image == nil {
                SVProgressHUD.showProgress(progress)
            }
        }
        
        SVProgressHUD.show(withStatus: "SCANNING".localized)
        Sane.shared.scan(device: device, progress: { (progress, image) in
            block(progress, false, image, nil, nil)
        }, completion: { (image, parameters, error) in
            block(1, true, image, parameters, error)
        })
    }
    
    @objc private func settingsButtonTap() {
        let nc = UINavigationController(rootViewController: PreferencesVC())
        nc.modalPresentationStyle = .formSheet
        present(nc, animated: true, completion: nil)
    }
    
    private func shareItem(_ item: GalleryItem?) {
        guard let url = item?.URL else { return }
        UIActivityViewController.showForURLs([url], fromBottomIn: self, completion: nil)
    }
    
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
}

// MARK: Snapshots
extension DeviceVC {
    private func prepareForSnapshotting() {
        let snapshotType = AppDelegate.obtain.snapshotType
        guard snapshotType != .none else { return }
    
        if snapshotType == .devicePreview || snapshotType == .deviceOptions || snapshotType == .deviceOptionPopup {
            let rect = CGRect(x: 0.1, y: 0.2, width: 0.8, height: 0.6)
            if let path = AppDelegate.obtain.snapshotTestScanImagePath {
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

        DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
            SVProgressHUD.dismiss()
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
        if section == 0 { return 1 }
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
        if let option = optionsInGroup(tableViewSection: indexPath.section)?[indexPath.row] {
            cell.updateWith(option: option)
        }
        cell.showDescription = false
        return cell
    }
    
    func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        if section == 0 { return "DEVICE SECTION PREVIEW".localized }
        return optionGroup(tableViewSection: section)?.localizedTitle
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
}

extension DeviceVC : UITableViewDelegate {
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        guard indexPath.section > 0, let option = optionsInGroup(tableViewSection: indexPath.section)?[indexPath.row] else { return }
        
        let vc = DeviceOptionVC(option: option)
        vc.closeBlock = {
            self.tableView.reloadData()
        }

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
    func sanePreviewView(_ sanePreviewView: SanePreviewView, tappedScan device: Device, updateBlock: ((UIImage?, Float, Bool) -> ())?) {
        updateBlock?(nil, 0, false)

        Sane.shared.preview(device: device, progress: { (progress, image) in
            updateBlock?(image, progress, false)
        }, completion: { (image, error) in
            updateBlock?(image, 1, true)
            if let error = error {
                SVProgressHUD.showError(withStatus: error.localizedDescription)
            }
        })
    }
    
    func sanePreviewView(_ sanePreviewView: SanePreviewView, canceledScan device: Device) {
        Sane.shared.cancelCurrentScan()
    }
}
