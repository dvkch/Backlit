//
//  AcknowledgementsVC.swift
//  SaneScanner
//
//  Created by syan on 19/03/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

class AcknowledgementsVC: UIViewController {
    
    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .background
        title = "PREFERENCES TITLE ACKNOWLEDGEMENTS".localized
        
        // prevent collapsing of navigationBar large title when scrolling
        view.addSubview(UIView())

        tableView.backgroundColor = .clear
        tableView.separatorStyle = UIDevice.isCatalyst ? .none : .singleLine
        tableView.tableHeaderView = UIView()
        tableView.tableFooterView = UIView()
        tableView.clipsToBounds = true
        tableView.alwaysBounceVertical = true
        tableView.dataSource = self
        tableView.delegate = self
        tableView.registerCell(AcknowledgementCell.self, xib: true)
        view.addSubview(tableView)
        tableView.snp.makeConstraints { make in
            make.top.bottom.equalTo(view)
            make.left.right.equalToSuperview()
        }
        
        navigationItem.rightBarButtonItem = .close(target: self, action: #selector(self.closeButtonTap))
        addKeyCommand(.close)

        tableView.reloadData()
    }
    
    // MARK: Properties
    private let acknowledgements = Acknowledgement.parsed
    private var deployedIndex: Int? {
        didSet {
            tableView.beginUpdates()
            if let oldValue {
                tableView.reloadRows(at: [IndexPath(row: oldValue, section: 0)], with: .automatic)
            }
            if let deployedIndex {
                tableView.reloadRows(at: [IndexPath(row: deployedIndex, section: 0)], with: .automatic)
            }
            tableView.endUpdates()
        }
    }
    
    // MARK: Views
    private let tableView = TableView(frame: CGRect(x: 0, y: 0, width: 320, height: 600), style: .grouped)

    // MARK: Actions
    @objc func closeButtonTap() {
        dismiss(animated: true, completion: nil)
    }
}

extension AcknowledgementsVC : UITableViewDataSource {
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return acknowledgements.count
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueCell(AcknowledgementCell.self, for: indexPath)
        cell.index = indexPath.row
        cell.acknowledgement = acknowledgements[indexPath.row]
        cell.showDescription = indexPath.row == deployedIndex
        return cell
    }
}

extension AcknowledgementsVC : UITableViewDelegate {
    func tableView(_ tableView: UITableView, estimatedHeightForRowAt indexPath: IndexPath) -> CGFloat {
        return 50
    }
    
    func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        return UITableView.automaticDimension
    }
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        if deployedIndex != indexPath.row {
            deployedIndex = indexPath.row
        }
        else {
            deployedIndex = nil
        }
    }
}
