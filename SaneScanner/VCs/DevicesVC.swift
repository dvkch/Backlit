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
        
        navigationItem.backBarButtonItem = UIBarButtonItem(title: "", style: .plain, target: nil, action: nil)
        if !UIDevice.isCatalyst {
            addKeyCommand(.settings)
            navigationItem.rightBarButtonItem = PreferencesVC.settingsBarButtonItem(target: self, action: #selector(self.settingsButtonTap))
        }

        // prevent collapsing of nnavigationBar large title when scrolling
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
            self?.refresh(afterHostChange: false)
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
        
        if devices.isEmpty {
            refresh(afterHostChange: false)
        }
    }
    
    // MARK: Views
    private let tableView = UITableView(frame: CGRect(x: 0, y: 0, width: 320, height: 600), style: .grouped)
    private var thumbsView: GalleryThumbsView!
    private var refreshView: RefreshView!
    
    // MARK: Properties
    private var devices = [Device]()
    private var loadingDevice: Device?
    private var hosts: [HostCell.Host] = []
    
    // MARK: Actions
    @objc func settingsButtonTap() {
        let nc = UINavigationController(rootViewController: PreferencesVC())
        nc.modalPresentationStyle = .formSheet
        present(nc, animated: true, completion: nil)
    }
    
    @objc func refresh(afterHostChange: Bool) {
        // refresh hosts
        var hosts: [HostCell.Host] = Sane.shared.configuration.hosts.map { .init(kind: .saneConfig, value: $0) }
        SaneBonjour.shared.hosts
            .sorted(by: \.0)
            .filter { !hosts.map(\.value).contains($0.0) }
            .forEach { hosts.append(.init(kind: .bonjour, value: $0.0)) }
        hosts.append(.init(kind: .add, value: ""))
        
        let prevSaneHosts = self.hosts.filter { $0.kind == .saneConfig }.map(\.value).sorted()
        let newSaneHosts = hosts.filter { $0.kind == .saneConfig }.map(\.value).sorted()
        self.hosts = hosts
        self.tableView.reloadData()

        if afterHostChange && prevSaneHosts == newSaneHosts {
            return
        }
        
        // refresh devices
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
    
    @objc func addHostButtonTap(bonjourSuggestion: String?) {
        Analytics.shared.send(event: .newHostTapped)

        let completion = { (host: String) in
            Sane.shared.configuration.addHost(host)
            self.tableView.reloadData()
            self.refresh(afterHostChange: true)
            Analytics.shared.send(event: .newHostAdded(
                count: Sane.shared.configuration.hosts.count,
                foundByAvahi: bonjourSuggestion != nil
            ))
        }
        #if targetEnvironment(macCatalyst)
        obtainCatalystPlugin().presentHostInputAlert(
            title: "DIALOG TITLE ADD HOST".localized,
            message: "DIALOG MESSAGE ADD HOST".localized,
            initial: bonjourSuggestion,
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
            field.text = bonjourSuggestion
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
        refreshView.startLoading()
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

extension DevicesVC : SaneBonjourDelegate {
    func saneBonjour(_ bonjour: SaneBonjour, updatedHosts: [(String, Int)]) {
        refresh(afterHostChange: true)
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
            cell.host = hosts[indexPath.row]
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
        
        let host = hosts[indexPath.row]
        guard host.kind == .saneConfig else { return nil }

        return UIContextMenuConfiguration(identifier: nil, previewProvider: nil) { (_) -> UIMenu? in
            let deleteAction = UIAction(title: "ACTION REMOVE".localized, image: UIImage(systemName: "trash.fill"), attributes: .destructive) { [weak self] (_) in
                Sane.shared.configuration.removeHost(host.value)
                
                CATransaction.begin()
                CATransaction.setCompletionBlock {
                    self?.refresh(afterHostChange: true)
                }
                tableView.beginUpdates()
                tableView.reloadSections(IndexSet(integer: 0), with: .none)
                tableView.endUpdates()
                CATransaction.commit()
            }

            return UIMenu(title: "", children: [deleteAction])
        }
    }

    #if !targetEnvironment(macCatalyst)
    func tableView(_ tableView: UITableView, editActionsForRowAt indexPath: IndexPath) -> [UITableViewRowAction]? {
        guard indexPath.section == 0 else { return nil }
        
        let host = hosts[indexPath.row]
        guard host.kind == .saneConfig else { return nil }

        let deleteAction = UITableViewRowAction(style: .destructive, title: "ACTION REMOVE".localized) { (_, indexPath) in
            Sane.shared.configuration.removeHost(host.value)

            CATransaction.begin()
            CATransaction.setCompletionBlock {
                self.refresh(afterHostChange: true)
            }
            tableView.beginUpdates()
            tableView.reloadSections(IndexSet(integer: 0), with: .none)
            tableView.endUpdates()
            CATransaction.commit()
        }
        return [deleteAction]
    }
    #endif
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        if indexPath.section == 0 {
            switch hosts[indexPath.row].kind {
            case .saneConfig:
                break

            case .bonjour:
                addHostButtonTap(bonjourSuggestion: hosts[indexPath.row].value)
                
            case .add:
                addHostButtonTap(bonjourSuggestion: nil)
            }
            return
        }
        
        guard loadingDevice == nil else { return }
        let device = devices[indexPath.row]

        loadingDevice = device
        (tableView.cellForRow(at: indexPath) as? DeviceCell)?.isLoading = true
        
        Sane.shared.openDevice(device) { (result) in
            self.loadingDevice = nil
            self.tableView.reloadData()

            if case .failure(let error) = result {
                if error.isSaneAuthDenied, DeviceAuthentication.saved(for: device.name.rawValue) != nil {
                    // this is an auth error, let's forget current auth and restart connexion
                    DeviceAuthentication.forget(for: device.name.rawValue)
                    self.tableView(tableView, didSelectRowAt: indexPath)
                    return
                }
                else {
                    UIAlertController.show(for: error, title: "DIALOG TITLE COULDNT OPEN DEVICE".localized, in: self)
                    return
                }
            }
            let vc = DeviceVC(device: device)
            self.navigationController?.pushViewController(vc, animated: true)
        }
    }
}
