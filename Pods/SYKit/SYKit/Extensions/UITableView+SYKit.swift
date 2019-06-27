//
//  UITableView+SYKit.swift
//  SYKit
//
//  Created by Stanislas Chevallier on 28/11/2018.
//  Copyright © 2018 Syan. All rights reserved.
//

import UIKit

public extension UITableView {

    func registerCell<T: UITableViewCell>(_ type: T.Type, xib: Bool = true) {
        let className = NSStringFromClass(type).components(separatedBy: ".").last!
        if xib {
            register(UINib(nibName: className, bundle: nil), forCellReuseIdentifier: className)
        }
        else {
            register(type, forCellReuseIdentifier: className)
        }
    }
    
    func registerHeader<T: UITableViewHeaderFooterView>(_ type: T.Type, xib: Bool = true) {
        let className = NSStringFromClass(type).components(separatedBy: ".").last!
        if xib {
            register(UINib(nibName: className, bundle: nil), forHeaderFooterViewReuseIdentifier: className)
        }
        else {
            register(type, forHeaderFooterViewReuseIdentifier: className)
        }
    }
    
    func dequeueCell(style: UITableViewCell.CellStyle) -> UITableViewCell {
        let identifier = "cell" + String(style.rawValue)
        return dequeueReusableCell(withIdentifier: identifier) ?? UITableViewCell(style: style, reuseIdentifier: identifier)
    }
    
    func dequeueCell<T: UITableViewCell>(_ type: T.Type, for indexPath: IndexPath) -> T {
        let className = NSStringFromClass(type).components(separatedBy: ".").last!
        return dequeueReusableCell(withIdentifier: className, for: indexPath) as! T
    }
    
    func dequeueHeader<T: UITableViewHeaderFooterView>(_ type: T.Type) -> T {
        let className = NSStringFromClass(type).components(separatedBy: ".").last!
        return dequeueReusableHeaderFooterView(withIdentifier: className) as! T
    }
}
