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
        navigationItem.largeTitleDisplayMode = .always
        
        navigationItem.backBarButtonItem = .emptyBack
        if !UIDevice.isCatalyst {
            addKeyCommand(.settings)
            navigationItem.rightBarButtonItem = .settings(target: self, action: #selector(self.settingsButtonTap))
        }

        // prevent collapsing of navigationBar large title when scrolling
        view.addSubview(UIView())
        
        if UIDevice.isCatalyst {
            tableView.contentInset.top = 18
            tableView.separatorStyle = .none
        }
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
        
        addKeyCommand(.refresh)
        refreshView = .init(tableView: tableView, viewController: self) { [weak self] in
            self?.refresh(silently: false)
        }

        addKeyCommand(.addHost)

        thumbsView = GalleryThumbsView.showInToolbar(of: self)

        Sane.shared.delegate = self
        SaneBonjour.shared.delegate = self
        SaneBonjour.shared.start()
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        title = Bundle.main.localizedName
        tableView.reloadData()
        
        refresh(silently: devices.count > 0)
    }
    
    // MARK: Views
    private let tableView = UITableView(frame: CGRect(x: 0, y: 0, width: 320, height: 600), style: .grouped)
    private var thumbsView: GalleryThumbsView!
    private var refreshView: RefreshView!
    
    // MARK: Properties
    private var devices = [Device]()
    private var loadingDevice: Device?
    private var hosts: [HostCell.Kind] = []
    
    // MARK: Actions
    @objc func settingsButtonTap() {
        let nc = UINavigationController(rootViewController: PreferencesVC())
        nc.modalPresentationStyle = .formSheet
        present(nc, animated: true, completion: nil)
    }
    
    @objc func refresh(silently: Bool) {
        let bonjourHosts = Sane.shared.configuration.transientdHosts
            .filter { !Sane.shared.configuration.hosts.contains($0) }

        // refresh hosts
        hosts = (
            Sane.shared.configuration.hosts.map { .saneHost($0) }
            + bonjourHosts.map { .bonjourHost($0) }
            + [.add]
        )
        tableView.reloadData()

        // refresh devices
        refreshView.startLoading(discreet: silently)

        SaneBonjour.shared.start()
        Sane.shared.updateDevices { [weak self] result in
            guard let self = self else { return }
            switch result {
            case .success(let devices):
                self.devices = devices
                self.tableView.reloadData()
            case .failure(let error):
                self.devices = []
                self.tableView.reloadData()
                UIAlertController.show(for: error, in: self)
            }
        }
    }
    
    @objc func addHostButtonTap() {
        showHostForm(.add(suggestion: nil))
    }

    private enum DeviceForm {
        case add(suggestion: SaneHost?)
        case edit(host: SaneHost)
    }
    private func showHostForm(_ kind: DeviceForm) {
        Analytics.shared.send(event: .newHostTapped)

        let initialHost: SaneHost?
        switch kind {
        case .add(let suggestion):  initialHost = suggestion
        case .edit(let host):       initialHost = host
        }
        
        let title: String
        switch kind {
        case .add(.none): title = "DIALOG ADD HOST TITLE".localized
        case .add(.some): title = "DIALOG PERSIST HOST TITLE".localized
        case .edit:       title = "DIALOG EDIT HOST TITLE".localized
        }

        
        let completion = { (hostname: String, displayName: String) in
            let newHost = SaneHost(hostname: hostname, displayName: displayName)
            switch kind {
            case .add(let suggestion):
                Sane.shared.configuration.hosts.append(newHost)
                Analytics.shared.send(event: .newHostAdded(
                    count: Sane.shared.configuration.hosts.count,
                    foundByAvahi: suggestion != nil
                ))
                
            case .edit(let originalHost):
                guard let index = Sane.shared.configuration.hosts.firstIndex(of: originalHost) else { return }
                Sane.shared.configuration.hosts[index] = newHost
                Analytics.shared.send(event: .hostEdited(
                    count: Sane.shared.configuration.hosts.count
                ))
            }
        }
        #if targetEnvironment(macCatalyst)
        obtainCatalystPlugin().presentHostInputAlert(
            title: title,
            message: "DIALOG ADD HOST MESSAGE".localized,
            hostPlaceholder: "DIALOG ADD HOST PLACEHOLDER HOST".localized,
            namePlaceholder: "DIALOG ADD HOST PLACEHOLDER NAME".localized,
            suggestedHost: initialHost?.hostname ?? "",
            suggestedName: initialHost?.displayName ?? "",
            add: "ACTION ADD".localized,
            cancel: "ACTION CANCEL".localized,
            completion: completion
        )
        #else
        let alert = UIAlertController(
            title: title,
            message: "DIALOG ADD HOST MESSAGE".localized,
            preferredStyle: .alert
        )
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.autocorrectionType = .no
            field.autocapitalizationType = .none
            field.keyboardType = .URL
            field.text = initialHost?.hostname
            field.placeholder = "DIALOG ADD HOST PLACEHOLDER HOST".localized
        }
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.autocorrectionType = .default
            field.autocapitalizationType = .words
            field.text = initialHost?.displayName
            field.placeholder = "DIALOG ADD HOST PLACEHOLDER NAME".localized
        }
        alert.addAction(UIAlertAction(title: "ACTION ADD".localized, style: .default, handler: { (_) in
            let host = alert.textFields?.first?.text ?? ""
            let name = alert.textFields?.last?.text ?? ""
            completion(host, name)
        }))
        alert.addAction(UIAlertAction(title: "ACTION CANCEL".localized, style: .cancel, handler: nil))
        present(alert, animated: true, completion: nil)
        #endif
    }
    
    func openDevice(_ device: Device) {
        let index = devices.firstIndex(of: device)
        openDevice(device, indexPath: index.map { IndexPath(row: $0, section: 1) })
    }
    
    private func openDevice(_ device: Device, indexPath: IndexPath?) {
        guard loadingDevice == nil else { return }

        loadingDevice = device
        if let indexPath, let cell = tableView.cellForRow(at: indexPath) as? DeviceCell {
            cell.isLoading = true
        }
        
        Sane.shared.openDevice(device) { (result) in
            self.loadingDevice = nil
            self.tableView.reloadData()

            if case .failure(let error) = result {
                if error.isSaneAuthDenied, DeviceAuthentication.saved(for: device.name.rawValue) != nil {
                    // this is an auth error, let's forget current auth and restart connexion
                    DeviceAuthentication.forget(for: device.name.rawValue)
                    if let indexPath {
                        self.tableView(self.tableView, didSelectRowAt: indexPath)
                    }
                    return
                }
                else {
                    UIAlertController.show(for: error, title: "DIALOG COULDNT OPEN DEVICE TITLE".localized, in: self)
                    return
                }
            }
            let vc = DeviceVC(device: device)
            self.navigationController?.pushViewController(vc, animated: true)
        }
    }
}

extension DevicesVC: SaneDelegate {
    func saneDidUpdateConfig(_ sane: Sane, previousConfig: SaneConfig) {
        let hostsChanged = (
            sane.configuration.hosts != previousConfig.hosts ||
            sane.configuration.transientdHosts != previousConfig.transientdHosts
        )
        if hostsChanged {
            refresh(silently: false)
        }
    }

    func saneDidStartUpdatingDevices(_ sane: Sane) {
    }
    
    func saneDidEndUpdatingDevices(_ sane: Sane) {
        refreshView.stopLoading()
    }
    
    func saneNeedsAuth(_ sane: Sane, for device: String?, completion: @escaping (DeviceAuthentication?) -> ()) {
        guard let device = device else { return }
        
        if let saved = DeviceAuthentication.saved(for: device) {
            completion(saved)
            return
        }

        #if targetEnvironment(macCatalyst)
        obtainCatalystPlugin().presentAuthInputAlert(
            title: "DIALOG AUTH TITLE".localized,
            message: "DIALOG AUTH MESSAGE %@".localized(device),
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
        let alert = UIAlertController(title: "DIALOG AUTH TITLE".localized, message: nil, preferredStyle: .alert)
        alert.message = "DIALOG AUTH MESSAGE %@".localized(device)
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

extension DevicesVC : SaneBonjourDelegate {
    func saneBonjour(_ bonjour: SaneBonjour, updatedHosts: [SaneHost]) {
        Sane.shared.configuration.transientdHosts = updatedHosts
    }
}

extension DevicesVC : UITableViewDataSource {
    func numberOfSections(in tableView: UITableView) -> Int {
        return 2
    }
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return section == 0 ? hosts.count : devices.count
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        if indexPath.section == 0 {
            let cell = tableView.dequeueCell(HostCell.self, for: indexPath)
            cell.kind = hosts[indexPath.row]
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
            header.text = "DEVICES SECTION DEVICES".localized
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
        guard case .saneHost(let host) = hosts[indexPath.row] else { return nil }

        return UIContextMenuConfiguration(identifier: nil, previewProvider: nil) { (_) -> UIMenu? in
            let editAction = UIAction(title: "ACTION EDIT".localized, image: UIImage(systemName: "pencil"), attributes: []) { [weak self] (_) in
                self?.showHostForm(.edit(host: host))
            }
            let deleteAction = UIAction(title: "ACTION REMOVE".localized, image: UIImage(systemName: "trash.fill"), attributes: .destructive) { (_) in
                Sane.shared.configuration.hosts.remove(host)
            }

            return UIMenu(title: "", children: [editAction, deleteAction])
        }
    }

    #if !targetEnvironment(macCatalyst)
    func tableView(_ tableView: UITableView, editActionsForRowAt indexPath: IndexPath) -> [UITableViewRowAction]? {
        guard indexPath.section == 0 else { return nil }
        guard case .saneHost(let host) = hosts[indexPath.row] else { return nil }

        let editAction = UITableViewRowAction(style: .default, title: "ACTION EDIT".localized) { [weak self] (_, _) in
            self?.showHostForm(.edit(host: host))
        }
        editAction.backgroundColor = .tint
        let deleteAction = UITableViewRowAction(style: .destructive, title: "ACTION REMOVE".localized) { (_, _) in
            Sane.shared.configuration.hosts.remove(host)
        }
        return [editAction, deleteAction]
    }
    #endif
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        if indexPath.section == 0 {
            switch hosts[indexPath.row] {
            case .saneHost:
                break

            case .bonjourHost(let host):
                showHostForm(.add(suggestion: host))
                
            case .add:
                showHostForm(.add(suggestion: nil))
            }
            return
        }

        openDevice(devices[indexPath.row], indexPath: indexPath)
    }
}
