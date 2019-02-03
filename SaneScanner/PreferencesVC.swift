//
//  PreferencesVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 03/02/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SYKit
import SYEmailHelper
import SYPair

@objc class PreferencesVC: UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        
        title = "PREFERENCES TITLE".localized
        view.backgroundColor = .groupTableViewBackground
        
        tableView.registerCell(OptionCell.self, xib: true)
        
        navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .stop, target: self, action: #selector(self.closeButtonTap))
        
        keysGroups = SYPreferences.shared.allKeysGrouped()
    }

    @objc static func settingsBarButtonItem(target: Any, action: Selector) -> UIBarButtonItem {
        return UIBarButtonItem(image: UIImage(named: "settings"), style: .plain, target: target, action: action)
    }

    // MARK: Properties
    private static let contactEmailAddress = "contact@syan.me"
    private var keysGroups = [SYPair<NSString, NSArray>]()

    // MARK: Views:
    @IBOutlet private var tableView: UITableView!
    
    // MARK: Actions
    @objc private func closeButtonTap() {
        dismiss(animated: true, completion: nil)
    }
    /*
    // MARK: - Navigation

    // In a storyboard-based application, you will often want to do a little preparation before navigation
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        // Get the new view controller using segue.destination.
        // Pass the selected object to the new view controller.
    }
    */

}

extension PreferencesVC : UITableViewDataSource {
    func numberOfSections(in tableView: UITableView) -> Int {
        return keysGroups.count + 1
    }
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        if section < keysGroups.count {
            return keysGroups[section].object2.count
        }
        return 2
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueCell(OptionCell.self, for: indexPath)
        if indexPath.section < keysGroups.count {
            let prefKey = keysGroups[indexPath.section].object2[indexPath.row] as! String
            cell.updateWith(prefKey: prefKey)
            cell.showDescription = true
        }
        else {
            if indexPath.row == 0 {
                cell.updateWith(leftText: "PREFERENCES TITLE CONTACT".localized, rightText: PreferencesVC.contactEmailAddress)
            }
            else {
                cell.updateWith(leftText: "PREFERENCES TITLE VERSION".localized, rightText: UIApplication.sy_appVersionAndBuild())
            }
        }
        return cell
    }
    
    func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        if section < keysGroups.count {
            return keysGroups[section].object1 as String?
        }
        return "PREFERENCES SECTION ABOUT APP"
    }
    
    
}

extension PreferencesVC : UITableViewDelegate {
    func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        if indexPath.section < keysGroups.count {
            let prefKey = keysGroups[indexPath.section].object2[indexPath.row] as! String
            return OptionCell.cellHeight(prefKey: prefKey, showDescription: true, width: tableView.bounds.width)
        }
        if indexPath.row == 0 {
            return OptionCell.cellHeight(leftText: "PREFERENCES TITLE CONTACT".localized, rightText: PreferencesVC.contactEmailAddress, width: tableView.bounds.width)
        }
        else {
            return OptionCell.cellHeight(leftText: "PREFERENCES TITLE VERSION".localized, rightText: UIApplication.sy_appVersionAndBuild(), width: tableView.bounds.width)
        }
    }
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        // ABOUT APP section
        if indexPath.section >= keysGroups.count {
            guard indexPath.row == 0 else { return }
            
            // TODO: cleanup
            var subject = String(format: "CONTACT SUBJECT ABOUT APP %@".localized, UIApplication.shared.sy_localizedName())
            subject += " " + UIApplication.sy_appVersionAndBuild()
            
            SYEmailServicePasteboard.name = "MAIL COPY PASTEBOARD NAME".localized
            SYEmailHelper.shared()?.composeEmail(
                withAddress: PreferencesVC.contactEmailAddress,
                subject: subject,
                body: nil,
                presentingVC: self) { (launched, service, error) in
                    // TODO: show HUD for pasteboard
                    print("Completion:", service?.name ?? "<no service>", launched, error?.localizedDescription ?? "<no error>")
            }
            return
        }
        
        let prefKey = keysGroups[indexPath.section].object2[indexPath.row] as! String
        if SYPreferences.shared.type(forKey: prefKey) == .bool {
            let previousValue = (SYPreferences.shared.object(forKey: prefKey) as! NSNumber).boolValue
            SYPreferences.shared.setObject(!previousValue, forKey: prefKey)
        }
        
        tableView.beginUpdates()
        tableView.reloadRows(at: [indexPath], with: .automatic)
        tableView.endUpdates()
    }
}
