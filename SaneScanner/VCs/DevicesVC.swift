//
//  DevicesVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 03/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SYKit

class DevicesVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .background
        
        navigationItem.backBarButtonItem = UIBarButtonItem(title: "", style: .plain, target: nil, action: nil)
        navigationItem.rightBarButtonItem = PreferencesVC.settingsBarButtonItem(target: self, action: #selector(self.settingsButtonTap))

        #if targetEnvironment(macCatalyst)
        tableView.contentInset.top = 18
        tableView.separatorStyle = .none
        #endif
        tableView.clipsToBounds = true
        tableView.alwaysBounceVertical = true
        tableView.dataSource = self
        tableView.delegate = self
        tableView.registerCell(HostCell.self, xib: true)
        tableView.registerCell(DeviceCell.self, xib: true)
        tableView.registerHeader(TableViewHeader.self, xib: false)
        view.addSubview(tableView)
        tableView.snp.makeConstraints { make in
            make.left.right.equalToSuperview()
            make.top.bottom.equalTo(view.layoutMarginsGuide)
        }
        
        loaderView = .init(tableView: tableView, viewController: self) { [weak self] in
            self?.refreshDevices()
        }

        thumbsView = GalleryThumbsView.showInToolbar(of: self, tintColor: nil)

        Sane.shared.delegate = self
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        title = Bundle.main.localizedName
        tableView.reloadData()
        
        if devices.isEmpty {
            refreshDevices()
        }
    }
    
    // MARK: Views
    private let tableView = UITableView(frame: CGRect(x: 0, y: 0, width: 320, height: 600), style: .grouped)
    private var thumbsView: GalleryThumbsView!
    private var loaderView: LoaderView!
    
    // MARK: Properties
    private var devices = [Device]()
    private var loadingDevice: Device?
    
    // MARK: Actions
    @objc func settingsButtonTap() {
        let nc = UINavigationController(rootViewController: PreferencesVC())
        nc.modalPresentationStyle = .formSheet
        present(nc, animated: true, completion: nil)
    }
    
    private func refreshDevices() {
        Sane.shared.updateDevices { [weak self] (devices, error) in
            guard let self = self else { return }
            self.devices = devices ?? []
            self.tableView.reloadData()
            
            if let error = error {
                UIAlertController.show(for: error, in: self)
            }
        }
    }
    
    private func addHostButtonTap() {
        let completion = { (host: String) in
            Sane.shared.configuration.addHost(host)
            self.tableView.reloadData()
            self.refreshDevices()
        }
        #if targetEnvironment(macCatalyst)
        obtainCatalystPlugin().presentHostInputAlert(
            title: "DIALOG TITLE ADD HOST".localized,
            message: "DIALOG MESSAGE ADD HOST".localized,
            add: "ACTION ADD".localized,
            cancel: "ACTION CANCEL".localized,
            completion: completion
        )
        #else
        let alert = UIAlertController(title: "DIALOG TITLE ADD HOST".localized, message: "DIALOG MESSAGE ADD HOST".localized, preferredStyle: .alert)
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.autocorrectionType = .no
            field.autocapitalizationType = .none
            field.keyboardType = .URL
        }
        alert.addAction(UIAlertAction(title: "ACTION ADD".localized, style: .default, handler: { (_) in
            let host = alert.textFields?.first?.text ?? ""
            completion(host)
        }))
        alert.addAction(UIAlertAction(title: "ACTION CANCEL".localized, style: .cancel, handler: nil))
        present(alert, animated: true, completion: nil)
        #endif
    }
}

extension DevicesVC: SaneDelegate {
    func saneDidStartUpdatingDevices(_ sane: Sane) {
        loaderView.startLoading()
    }
    
    func saneDidEndUpdatingDevices(_ sane: Sane) {
        loaderView.stopLoading()
    }
    
    func saneNeedsAuth(_ sane: Sane, for device: String?, completion: @escaping (DeviceAuthentication?) -> ()) {
        guard let device = device else { return }
        
        if let saved = DeviceAuthentication.saved(for: device) {
            completion(saved)
            return
        }

        #if targetEnvironment(macCatalyst)
        obtainCatalystPlugin().presentAuthInputAlert(
            title: "DIALOG TITLE AUTH".localized,
            message: String(format: "DIALOG MESSAGE AUTH %@".localized, device),
            usernamePlaceholder: "DIALOG AUTH PLACEHOLDER USERNAME".localized,
            passwordPlaceholder: "DIALOG AUTH PLACEHOLDER PASSWORD".localized,
            continue: "ACTION CONTINUE".localized,
            remember: "ACTION CONTINUE REMEMBER".localized,
            cancel: "ACTION CANCEL".localized
        ) { (username, password, remember) in
            if let username = username, let password = password {
                let auth = DeviceAuthentication(username: username, password: password)
                if remember {
                    auth.save(for: device)
                }
                completion(auth)
            }
            else {
                completion(nil)
            }
        }
        #else
        let alert = UIAlertController(title: "DIALOG TITLE AUTH".localized, message: nil, preferredStyle: .alert)
        alert.message = String(format: "DIALOG MESSAGE AUTH %@".localized, device)
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.placeholder = "DIALOG AUTH PLACEHOLDER USERNAME".localized
            field.autocorrectionType = .no
        }
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.placeholder = "DIALOG AUTH PLACEHOLDER PASSWORD".localized
            field.isSecureTextEntry = true
            field.autocorrectionType = .no
        }
        alert.addAction(UIAlertAction(title: "ACTION CONTINUE".localized, style: .default, handler: { (_) in
            let username = alert.textFields?.first?.text
            let password = alert.textFields?.last?.text
            completion(DeviceAuthentication(username: username, password: password))
        }))
        alert.addAction(UIAlertAction(title: "ACTION CONTINUE REMEMBER".localized, style: .default, handler: { (_) in
            let username = alert.textFields?.first?.text
            let password = alert.textFields?.last?.text
            let auth = DeviceAuthentication(username: username, password: password)
            auth.save(for: device)
            completion(auth)
        }))
        alert.addAction(UIAlertAction(title: "ACTION CANCEL".localized, style: .cancel, handler: { (_) in
            completion(nil)
        }))
        present(alert, animated: true, completion: nil)
        #endif
    }
}

extension DevicesVC : UITableViewDataSource {
    func numberOfSections(in tableView: UITableView) -> Int {
        return 2
    }
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return section == 0 ? Sane.shared.configuration.hosts.count + 1 : devices.count
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        if indexPath.section == 0 {
            let cell = tableView.dequeueCell(HostCell.self, for: indexPath)
            if indexPath.row < Sane.shared.configuration.hosts.count {
                cell.title = Sane.shared.configuration.hosts[indexPath.row]
                cell.showAddIndicator = false
            }
            else {
                cell.title = "DEVICES ROW ADD HOST".localized
                cell.showAddIndicator = true
            }
            return cell
        }
        
        let cell = tableView.dequeueCell(DeviceCell.self, for: indexPath)
        cell.device = devices[indexPath.row]
        cell.isLoading = cell.device == loadingDevice
        return cell
    }
    
    func tableView(_ tableView: UITableView, viewForHeaderInSection section: Int) -> UIView? {
        let header = tableView.dequeueHeader(TableViewHeader.self)
        if section == 0 {
            header.text = "DEVICES SECTION HOSTS".localized
        }
        else {
            header.text = "DEVICES SECTION DEVICES".localized;
        }
        return header
    }
}

extension DevicesVC : UITableViewDelegate {
    func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        return UITableView.automaticDimension
    }
    
    @available(iOS 13.0, *)
    func tableView(_ tableView: UITableView, contextMenuConfigurationForRowAt indexPath: IndexPath, point: CGPoint) -> UIContextMenuConfiguration? {
        guard indexPath.section == 0 else { return nil }
        guard indexPath.row < Sane.shared.configuration.hosts.count else { return nil }
        
        return UIContextMenuConfiguration(identifier: nil, previewProvider: nil) { (_) -> UIMenu? in
            let deleteAction = UIAction(title: "ACTION REMOVE".localized, image: UIImage(systemName: "trash.fill"), attributes: .destructive) { [weak self] (_) in
                Sane.shared.configuration.removeHost(Sane.shared.configuration.hosts[indexPath.row])
                
                CATransaction.begin()
                CATransaction.setCompletionBlock {
                    self?.refreshDevices()
                }
                tableView.beginUpdates()
                tableView.deleteRows(at: [indexPath], with: .bottom)
                tableView.endUpdates()
                CATransaction.commit()
            }

            return UIMenu(title: "", children: [deleteAction])
        }
    }

    #if !targetEnvironment(macCatalyst)
    func tableView(_ tableView: UITableView, editActionsForRowAt indexPath: IndexPath) -> [UITableViewRowAction]? {
        guard indexPath.section == 0 else { return nil }
        guard indexPath.row < Sane.shared.configuration.hosts.count else { return nil }
        
        let deleteAction = UITableViewRowAction(style: .destructive, title: "ACTION REMOVE".localized) { (_, indexPath) in
            Sane.shared.configuration.removeHost(Sane.shared.configuration.hosts[indexPath.row])
            
            CATransaction.begin()
            CATransaction.setCompletionBlock {
                self.refreshDevices()
            }
            tableView.beginUpdates()
            tableView.deleteRows(at: [indexPath], with: .bottom)
            tableView.endUpdates()
            CATransaction.commit()
        }
        return [deleteAction]
    }
    #endif
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        if indexPath.section == 0 {
            guard indexPath.row >= Sane.shared.configuration.hosts.count else { return }
            addHostButtonTap()
            return
        }
        
        guard loadingDevice == nil else { return }
        let device = devices[indexPath.row]
        loadingDevice = device
        (tableView.cellForRow(at: indexPath) as? DeviceCell)?.isLoading = true
        
        Sane.shared.openDevice(device) { (error) in
            self.loadingDevice = nil
            if let error = error {
                if let error = (error as? SaneError), case let .saneError(status) = error, status.rawValue == 11, DeviceAuthentication.saved(for: device.name) != nil {
                    // this is an auth error, let's forget current auth and restart connexion
                    DeviceAuthentication.forget(for: device.name)
                    self.tableView(tableView, didSelectRowAt: indexPath)
                    return
                }

                UIAlertController.show(for: error, title: "DIALOG TITLE COULDNT OPEN DEVICE".localized, in: self)
                return
            }
            let vc = DeviceVC(device: device)
            self.navigationController?.pushViewController(vc, animated: true)
        }
    }
}
