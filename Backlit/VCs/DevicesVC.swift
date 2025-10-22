//
//  DevicesVC.swift
//  Backlit
//
//  Created by syan on 03/02/2019.
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
        navigationItem.backButtonDisplayMode = .minimal
        navigationController?.navigationBar.setBackButtonImage(.icon(.left))

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
                var filteredDevices = devices
                Snapshot.setup { _ in
                    filteredDevices = filteredDevices.filter { $0.model != "frontend-tester" }
                }
                self.devices = filteredDevices
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
        case .add(.none): title = L10n.dialogAddHostTitle
        case .add(.some): title = L10n.dialogPersistHostTitle
        case .edit:       title = L10n.dialogEditHostTitle
        }

        
        let completion = { (hostname: String, displayName: String) in
            guard let hostname = hostname.trimmingCharacters(in: .whitespacesAndNewlines).nilIfEmpty else { return }

            let newHost = SaneHost(hostname: hostname, displayName: displayName.trimmingCharacters(in: .whitespacesAndNewlines))
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
            message: L10n.dialogAddHostMessage,
            hostPlaceholder: L10n.dialogAddHostPlaceholderHost,
            namePlaceholder: L10n.dialogAddHostPlaceholderName,
            suggestedHost: initialHost?.hostname ?? "",
            suggestedName: initialHost?.displayName ?? "",
            add: L10n.actionAdd,
            cancel: L10n.actionCancel,
            completion: completion
        )
        #else
        let alert = UIAlertController(
            title: title,
            message: L10n.dialogAddHostMessage,
            preferredStyle: .alert
        )
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.autocorrectionType = .no
            field.autocapitalizationType = .none
            field.keyboardType = .URL
            field.text = initialHost?.hostname
            field.placeholder = L10n.dialogAddHostPlaceholderHost
        }
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.autocorrectionType = .default
            field.autocapitalizationType = .words
            field.text = initialHost?.displayName
            field.placeholder = L10n.dialogAddHostPlaceholderName
        }
        alert.addAction(UIAlertAction(title: L10n.actionAdd, style: .default, handler: { (_) in
            let host = alert.textFields?.first?.text ?? ""
            let name = alert.textFields?.last?.text ?? ""
            completion(host, name)
        }))
        alert.addAction(UIAlertAction(title: L10n.actionCancel, style: .cancel, handler: nil))
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
                    UIAlertController.show(for: error, title: L10n.dialogCouldntOpenDeviceTitle, in: self)
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
            title: L10n.dialogAuthTitle,
            message: L10n.dialogAuthMessage(device),
            usernamePlaceholder: L10n.dialogAuthPlaceholderUsername,
            passwordPlaceholder: L10n.dialogAuthPlaceholderPassword,
            continue: L10n.actionContinue,
            remember: L10n.actionContinueRemember,
            cancel: L10n.actionCancel
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
        let alert = UIAlertController(title: L10n.dialogAuthTitle, message: nil, preferredStyle: .alert)
        alert.message = L10n.dialogAuthMessage(device)
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.placeholder = L10n.dialogAuthPlaceholderUsername
            field.autocorrectionType = .no
        }
        alert.addTextField { (field) in
            field.borderStyle = .none
            field.placeholder = L10n.dialogAuthPlaceholderPassword
            field.isSecureTextEntry = true
            field.autocorrectionType = .no
        }
        alert.addAction(UIAlertAction(title: L10n.actionContinue, style: .default, handler: { (_) in
            let username = alert.textFields?.first?.text
            let password = alert.textFields?.last?.text
            completion(DeviceAuthentication(username: username, password: password))
        }))
        alert.addAction(UIAlertAction(title: L10n.actionContinueRemember, style: .default, handler: { (_) in
            let username = alert.textFields?.first?.text
            let password = alert.textFields?.last?.text
            let auth = DeviceAuthentication(username: username, password: password)
            auth.save(for: device)
            completion(auth)
        }))
        alert.addAction(UIAlertAction(title: L10n.actionCancel, style: .cancel, handler: { (_) in
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
        cell.index = indexPath.row
        cell.isLoading = cell.device == loadingDevice
        return cell
    }
    
    private func headerText(in section: Int) -> String {
        if section == 0 {
            return L10n.devicesSectionHosts
        }
        else {
            return L10n.devicesSectionDevices
        }
    }
    
    func tableView(_ tableView: UITableView, viewForHeaderInSection section: Int) -> UIView? {
        let header = tableView.dequeueHeader(TableViewHeader.self)
        header.text = headerText(in: section)
        header.topMargin = section == 0 ? 17 : 0
        return header
    }
    
    func tableView(_ tableView: UITableView, heightForHeaderInSection section: Int) -> CGFloat {
        let text = headerText(in: section)
        return TableViewHeader.height(for: text, topMargin: section == 0 ? 17 : 0, at: section, in: tableView)
    }
}

extension DevicesVC : UITableViewDelegate {
    func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        return UITableView.automaticDimension
    }
    
    func tableView(_ tableView: UITableView, contextMenuConfigurationForRowAt indexPath: IndexPath, point: CGPoint) -> UIContextMenuConfiguration? {
        guard indexPath.section == 0 else { return nil }
        guard case .saneHost(let host) = hosts[indexPath.row] else { return nil }

        return UIContextMenuConfiguration(identifier: nil, previewProvider: nil) { (_) -> UIMenu? in
            let editAction = UIAction(title: L10n.actionEdit, image: .icon(.edit)) { [weak self] (_) in
                self?.showHostForm(.edit(host: host))
            }
            let deleteAction = UIAction(title: L10n.actionRemove, image: .icon(.delete), attributes: .destructive) { (_) in
                Sane.shared.configuration.hosts.remove(host)
            }

            return UIMenu(title: "", children: [editAction, deleteAction])
        }
    }

    func tableView(_ tableView: UITableView, trailingSwipeActionsConfigurationForRowAt indexPath: IndexPath) -> UISwipeActionsConfiguration? {
        guard indexPath.section == 0 else { return nil }
        guard case .saneHost(let host) = hosts[indexPath.row] else { return nil }

        let editAction = UIContextualAction(style: .normal, title: L10n.actionEdit) { [weak self] (_, _, complete) in
            self?.showHostForm(.edit(host: host))
            complete(true)
        }
        editAction.image = .icon(.edit)
        editAction.backgroundColor = .tint

        let deleteAction = UIContextualAction(style: .destructive, title: L10n.actionRemove) { (_, _, complete) in
            Sane.shared.configuration.hosts.remove(host)
            complete(true)
        }
        deleteAction.image = .icon(.delete)
        return UISwipeActionsConfiguration(actions: [deleteAction, editAction])
    }
    
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
