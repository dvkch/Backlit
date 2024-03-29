//
//  PreferencesVC.swift
//  Backlit
//
//  Created by Stanislas Chevallier on 03/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit
import SYEmailHelper
import SaneSwift

class PreferencesVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()

        if #available(iOS 13.0, *) {
            navigationController?.navigationBar.scrollEdgeAppearance = .init()
            navigationController?.navigationBar.scrollEdgeAppearance?.configureWithDefaultBackground()
            navigationController?.navigationBar.prefersLargeTitles = true
        }
        navigationItem.largeTitleDisplayMode = .always
        navigationItem.backBarButtonItem = .back(title: "PREFERENCES TITLE".localized)
        navigationController?.navigationBar.setBackButtonImage(.icon(.left))

        title = "PREFERENCES TITLE".localized
        view.backgroundColor = .background
        
        if UIDevice.isCatalyst {
            tableView.contentInset.top = 18
            tableView.separatorStyle = .none
        }
        tableView.registerHeader(TableViewHeader.self, xib: false)
        tableView.registerCell(OptionCell.self, xib: true)
        
        navigationItem.rightBarButtonItem = .close(target: self, action: #selector(self.closeButtonTap))
        addKeyCommand(.close)
        
        NotificationCenter.default.addObserver(self, selector: #selector(prefsChanged), name: .preferencesChanged, object: nil)

        updateCacheSize()
    }

    // MARK: Properties
    private enum Section {
        case prefGroup(title: String, keys: [Preferences.Key])
        case misc(rows: [MiscRow])
        case about(rows: [AboutRow])
        
        enum MiscRow: CaseIterable {
            case cleanCache
        }

        enum AboutRow: CaseIterable {
            case appVersion, saneVersion, contact, acknowledgements
        }
        
        static var allSections: [Section] = {
            let prefs = Preferences.shared.groupedKeys.map { Section.prefGroup(title: $0.0, keys: $0.1) }
            let misc = Section.misc(rows: MiscRow.allCases)
            let about = Section.about(rows: AboutRow.allCases)
            return prefs + [misc, about]
        }()
    }
    private static let contactEmailAddress = "contact@syan.me"
    private var cacheSize: String? = nil {
        didSet {
            tableView.reloadData()
        }
    }

    // MARK: Views:
    @IBOutlet private var tableView: UITableView!
    
    // MARK: Actions
    @objc func closeButtonTap() {
        dismiss(animated: true, completion: nil)
    }
    
    @objc private func prefsChanged() {
        UIView.performWithoutAnimation {
            tableView.reloadData()
        }
    }
    
    // MARK: Content
    private func updateCacheSize() {
        FileManager.default.sizeOfCacheDirectories { [weak self] size in
            self?.cacheSize = ByteCountFormatter().string(fromByteCount: size)
        }
    }
}

extension PreferencesVC : UITableViewDataSource {
    func numberOfSections(in tableView: UITableView) -> Int {
        return Section.allSections.count
    }
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section.allSections[section] {
        case .prefGroup(_, let keys):
            return keys.count
        case .misc(let rows):
            return rows.count
        case .about(let rows):
            return rows.count
        }
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueCell(OptionCell.self, for: indexPath)
        cell.accessoryType = .none
        cell.accessoryView = nil
        
        switch Section.allSections[indexPath.section] {
        case .prefGroup(_, let keys):
            cell.updateWith(prefKey: keys[indexPath.row])
            cell.showDescription = true

        case .misc(let rows):
            switch rows[indexPath.row] {
            case .cleanCache:
                cell.updateWith(
                    leftText: "PREFERENCES TITLE CLEANUP CACHE".localized,
                    rightText: cacheSize ?? "...",
                    description: "PREFERENCES MESSAGE CLEANUP CACHE".localized
                )
            }
            
        case .about(let rows):
            switch rows[indexPath.row] {
            case .appVersion:
                cell.updateWith(    
                    leftText: "PREFERENCES TITLE APP VERSION".localized,
                    rightText: Bundle.main.fullVersion
                )
            case .saneVersion:
                cell.updateWith(
                    leftText: "PREFERENCES TITLE SANE VERSION".localized,
                    rightText: Sane.shared.saneVersion.stringVersion
                )
            case .contact:
                cell.updateWith(
                    leftText: "PREFERENCES TITLE CONTACT".localized,
                    rightText: PreferencesVC.contactEmailAddress
                )
            case .acknowledgements:
                cell.updateWith(
                    leftText: "PREFERENCES TITLE ACKNOWLEDGEMENTS".localized,
                    rightText: ""
                )
                cell.showDisclosureIndicator(index: 0)
            }
        }
        return cell
    }
    
    func tableView(_ tableView: UITableView, viewForHeaderInSection section: Int) -> UIView? {
        let header = tableView.dequeueHeader(TableViewHeader.self)

        switch Section.allSections[section] {
        case .prefGroup(let title, _):
            header.text = title
        case .misc(_):
            header.text = "PREFERENCES SECTION MISC".localized
        case .about(_):
            header.text = "PREFERENCES SECTION ABOUT APP".localized
        }
        return header
    }
}

extension PreferencesVC : UITableViewDelegate {
    func tableView(_ tableView: UITableView, estimatedHeightForRowAt indexPath: IndexPath) -> CGFloat {
        switch Section.allSections[indexPath.section] {
        case .prefGroup(_, let keys):
            return OptionCell.cellHeight(prefKey: keys[indexPath.row], showDescription: true, width: tableView.bounds.width)
        case .misc, .about:
            return OptionCell.cellHeight(leftText: "TEST TITLE", rightText: "TEST VALUE", width: tableView.bounds.width)
        }
    }
    
    func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        return UITableView.automaticDimension
    }
    
    func tableView(_ tableView: UITableView, estimatedHeightForHeaderInSection section: Int) -> CGFloat {
        return UITableView.automaticDimension
    }
    
    func tableView(_ tableView: UITableView, heightForHeaderInSection section: Int) -> CGFloat {
        return UITableView.automaticDimension
    }
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        let cell = tableView.cellForRow(at: indexPath) as? OptionCell
        
        switch Section.allSections[indexPath.section] {
        case .prefGroup(_, let keys):
            if cell?.hasControlToUpdateItsValue == false {
                Preferences.shared.toggle(key: keys[indexPath.row])
            }
            
        case .misc(let rows):
            switch rows[indexPath.row] {
            case .cleanCache:
                FileManager.default.emptyCacheDirectories()
                updateCacheSize()
            }

        case .about(let rows):
            switch rows[indexPath.row] {
            case .appVersion, .saneVersion:
                break
                
            case .contact:
                let subject = "CONTACT SUBJECT ABOUT APP %@ %@".localized(Bundle.main.localizedName ?? "", Bundle.main.fullVersion)
                
                PasteboardEmailService.name = "MAIL COPY PASTEBOARD NAME".localized
                EmailHelper.shared.actionSheetTitle = "MAIL ALERT TITLE".localized
                EmailHelper.shared.actionSheetMessage = "MAIL ALERT MESSAGE".localized
                EmailHelper.shared.actionSheetCancelButtonText = "MAIL ALERT CANCEL".localized
                EmailHelper.shared.presentActionSheet(
                    address: PreferencesVC.contactEmailAddress,
                    subject: subject,
                    body: nil,
                    presentingViewController: self,
                    sender:
                    tableView.cellForRow(at: indexPath))
                { (launched, service, error) in
                    if service is PasteboardEmailService {
                        UIAlertController.show(message: "MAIL COPY PASTEBOARD SUCCESS".localized, in: self)
                    }
                    if let error {
                        Logger.e(.app, "EmailHelper \(service?.name ?? "<no service>"): \(error)")
                    }
                    else {
                        Logger.e(.app, "EmailHelper \(service?.name ?? "<no service>"): launched=\(launched)")
                    }
                }
                
            case .acknowledgements:
                navigationController?.pushViewController(AcknowledgementsVC(), animated: true)
            }
        }
    }
}
