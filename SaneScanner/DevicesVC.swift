//
//  DevicesVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 03/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import DLAlertView
import SVProgressHUD
import SYKit

@objc class DevicesVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .groupTableViewBackground
        
        thumbsView = SYGalleryThumbsView.show(inToolbarOf: self, tintColor: nil)
        
        tableView.registerCell(HostCell.self, xib: true)
        tableView.registerCell(DeviceCell.self, xib: true)
        tableView.sy_addPullToResfresh { [weak self] (_) in
            self?.refreshDevices()
        }
        
        navigationItem.rightBarButtonItem = PreferencesVC.settingsBarButtonItem(target: self, action: #selector(self.settingsButtonTap))
        
        Sane.shared.delegate = self
        
        sy_setBackButton(withText: nil, font: nil)
    }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        title = Bundle.main.localizedName
        tableView.reloadData()
        
        // TODO: remove ability to call block, should run Sane refresh instead that itself triggers this
        if devices.isEmpty {
            refreshDevices()
        }
    }
    
    // MARK: Views
    @IBOutlet private var tableView: UITableView!
    private var thumbsView: SYGalleryThumbsView!
    
    // MARK: Properties
    private var devices = [SYSaneDevice]()
    
    // MARK: Actions
    @objc private func settingsButtonTap() {
        let nc = UINavigationController(rootViewController: PreferencesVC())
        nc.modalPresentationStyle = .formSheet
        present(nc, animated: true, completion: nil)

    }
    
    private func refreshDevices() {
        Sane.shared.updateDevices { [weak self] (devices, error) in
            guard let self = self else { return }
            self.devices = devices ?? []
            self.tableView.reloadData()
            
            // in case it was opened (e.g. for screenshots)
            SVProgressHUD.dismiss()

            if let error = error {
                SVProgressHUD.showError(withStatus: (error as NSError).sy_alertMessage())
            }
        }
    }
}

extension DevicesVC: SaneDelegate {
    func saneDidStartUpdatingDevices(_ sane: Sane) {
        tableView.sy_showPull(toRefreshAndRunBlock: false)
    }
    
    func saneDidEndUpdatingDevices(_ sane: Sane) {
        tableView.sy_endPullToRefresh()
    }
    
    func saneNeedsAuth(_ sane: Sane, for device: String) -> DeviceAuthentication? {
        return nil
        // TODO: do auth but with completion block
        /*
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        
        __block NSString *outUsername;
        __block NSString *outPassword;
        
        dispatch_async(dispatch_get_main_queue(), ^
            {
                DLAVAlertView *alertView =
                    [[DLAVAlertView alloc] initWithTitle:$("DIALOG TITLE AUTH")
                        message:[NSString stringWithFormat:$("DIALOG MESSAGE AUTH %@"), device]
                        delegate:nil
                        cancelButtonTitle:$("ACTION CANCEL")
                        otherButtonTitles:$("ACTION CONTINUE"), nil];
                
                [alertView setAlertViewStyle:DLAVAlertViewStyleLoginAndPasswordInput];
                [[alertView textFieldAtIndex:0] setBorderStyle:UITextBorderStyleNone];
                [[alertView textFieldAtIndex:1] setBorderStyle:UITextBorderStyleNone];
                [alertView showWithCompletion:^(DLAVAlertView *alertView, NSInteger buttonIndex) {
                    if (buttonIndex != alertView.cancelButtonIndex)
                    {
                    outUsername = [alertView textFieldAtIndex:0].text;
                    outPassword = [alertView textFieldAtIndex:1].text;
                    }
                    dispatch_semaphore_signal(semaphore);
                    }];
            });
        
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        
        return [[DeviceAuthentication alloc] initWithUsername:outUsername password:outPassword];
        */
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
        return cell
    }
    
    func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        return section == 0 ? "DEVICES SECTION HOSTS".localized : "DEVICES SECTION DEVICES".localized;
    }
}

extension DevicesVC : UITableViewDelegate {
    func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        // TODO: use auto
        return indexPath.section == 0 ? 44 : 52
    }
    
    func tableView(_ tableView: UITableView, editActionsForRowAt indexPath: IndexPath) -> [UITableViewRowAction]? {
        guard indexPath.section == 0 else { return nil }
        guard indexPath.row < Sane.shared.configuration.hosts.count else { return nil }
        
        let deleteAction = UITableViewRowAction(style: .destructive, title: "ACTION REMOVE".localized) { (_, indexPath) in
            Sane.shared.configuration.removeHost(Sane.shared.configuration.hosts[indexPath.row])
            
            tableView.beginUpdates()
            tableView.deleteRows(at: [indexPath], with: .bottom)
            tableView.endUpdates()
            tableView.sy_showPull(toRefreshAndRunBlock: true)
        }
        return [deleteAction]
    }
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        if indexPath.section == 0 {
            guard indexPath.row >= Sane.shared.configuration.hosts.count else { return }
            let av = DLAVAlertView(
                title: "DIALOG TITLE ADD HOST".localized,
                message: "DIALOG MESSAGE ADD HOST".localized,
                delegate: nil,
                cancel: "ACTION CANCEL".localized,
                others: ["ACTION ADD".localized]
            )
            
            av.alertViewStyle = .plainTextInput
            av.textField(at: 0)?.borderStyle = .none
            av.show { (alert, index) in
                guard index != alert?.cancelButtonIndex else { return }
                let host = av.textField(at: 0)?.text ?? ""
                Sane.shared.configuration.addHost(host)
                self.tableView.reloadData()
                self.refreshDevices()
            }
            return;
        }
        
        let device = devices[indexPath.row]
        SVProgressHUD.show(withStatus: "LOADING".localized)
        
        Sane.shared.openDevice(device) { (error) in
            if AppDelegate.obtain.snapshotType == .none {
                SVProgressHUD.dismiss()
            }
            
            if let error = error {
                let alert = UIAlertController(
                    title: "DIALOG TITLE COULDNT OPEN DEVICE".localized,
                    message: (error as NSError).sy_alertMessage(),
                    preferredStyle: .alert
                )
                alert.addAction(UIAlertAction(title: "ACTION CLOSE".localized, style: .default, handler: nil))
                self.present(alert, animated: true, completion: nil)
                return
            }
            let vc = DeviceVC(device: device)
            self.navigationController?.pushViewController(vc, animated: true)
        }
    }
}
