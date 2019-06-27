//
//  NSAttributedString+SYKit.m
//  SYKit
//
//  Created by Stan Chevallier on 07/09/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

import Foundation
import UIKit

// MARK: Convenience inits
public extension NSAttributedString {
    @objc(sy_initWithText:font:color:)
    convenience init?(string: String?, font: UIFont?, color: UIColor? = nil) {
        guard let string = string else { return nil }
        self.init(string: string, font: font, color: color)
    }
    
    @nonobjc
    convenience init(string: String, font: UIFont?, color: UIColor? = nil) {
        var styles: [NSAttributedString.Key: Any] = [:]
        if let font = font {
            styles[.font] = font
        }
        if let color = color {
            styles[.foregroundColor] = color
        }
        
        self.init(string: string, attributes: styles)
    }
    
    @objc(sy_initWithImage:offset:)
    convenience init?(image: UIImage?, offset: CGPoint = .zero) {
        guard let image = image else { return nil }
        let attachment = NSTextAttachment()
        attachment.image = image
        attachment.bounds = CGRect(origin: offset, size: image.size)
        self.init(attachment: attachment)
    }
}

// MARK: Trimming
public extension NSAttributedString {
    @objc(attributedStringByTrimmingCharactersIn:)
    func trimmingCharacters(in charSet: CharacterSet) -> NSAttributedString {
        let modifiedString = NSMutableAttributedString(attributedString: self)
        modifiedString.trimCharacters(in: charSet)
        return NSAttributedString(attributedString: modifiedString)
    }
}

public extension NSMutableAttributedString {
    @objc(trimCharactersIn:)
    func trimCharacters(in charSet: CharacterSet) {
        var range = (string as NSString).rangeOfCharacter(from: charSet)
        
        // Trim leading characters from character set.
        while range.length != 0 && range.location == 0 {
            replaceCharacters(in: range, with: "")
            range = (string as NSString).rangeOfCharacter(from: charSet)
        }
        
        // Trim trailing characters from character set.
        range = (string as NSString).rangeOfCharacter(from: charSet, options: .backwards)
        while range.length != 0 && NSMaxRange(range) == length {
            replaceCharacters(in: range, with: "")
            range = (string as NSString).rangeOfCharacter(from: charSet, options: .backwards)
        }
    }
}

// MARK: Styling
public extension NSMutableAttributedString {
    @discardableResult
    @objc(sy_setAlignment:lineSpacing:paragraphSpacing:)
    func setParagraphStyle(alignment: NSTextAlignment = .natural, lineSpacing: CGFloat, paragraphSpacing: CGFloat) -> NSMutableAttributedString {
        let paragraphStyle = NSParagraphStyle.default.mutableCopy() as! NSMutableParagraphStyle
        paragraphStyle.lineSpacing = lineSpacing
        paragraphStyle.paragraphSpacing = paragraphSpacing
        paragraphStyle.alignment = alignment
        addAttribute(.paragraphStyle, value: paragraphStyle, range: NSRange(location: 0, length: length))
        return self
    }
}

// MARK: Sizing
public extension NSAttributedString {
    private var sizingLabel: UILabel {
        struct Static {
            static let instance: UILabel = {
                let instance = UILabel()
                instance.numberOfLines = 0
                return instance
            }()
        }
        
        return Static.instance
    }
    
    @objc(sy_sizeInBoundingSize:)
    func size(boundingSize: CGSize) -> CGSize {
        guard Thread.isMainThread else {
            print("Using \(#function) in thread other than main thread (\(Thread.current))")
            return .zero
        }
        
        sizingLabel.attributedText = self
        let size = sizingLabel.sizeThatFits(boundingSize)
        return CGSize(width: ceil(size.width), height: ceil(size.height))
    }
    
    @objc(sy_sizeInBoundingWidth:)
    func size(boundingWidth: CGFloat) -> CGSize {
        return size(boundingSize: CGSize(width: boundingWidth, height: CGFloat.greatestFiniteMagnitude))
    }
}

// MARK: Concat
public extension NSAttributedString {
    static func +(lhs: NSAttributedString, rhs: NSAttributedString?) -> NSAttributedString {
        return [lhs, rhs].concat(separator: nil)
    }
}

public extension NSMutableAttributedString {
    static func +=(lhs: NSMutableAttributedString, rhs: NSAttributedString?) {
        if let rhs = rhs {
            lhs.append(rhs)
        }
    }
    
    @objc(sy_append:)
    func append(_ string: NSAttributedString?) {
        if let string = string {
            self.append(string)
        }
    }
    
    @objc(sy_appendString:font:color:)
    func append(_ string: String?, font: UIFont? = nil, color: UIColor? = nil) {
        self.append(NSAttributedString(string: string, font: font, color: color))
    }
}

extension Collection where Element : NSAttributedString {
    func concat(separator: String? = nil) -> NSMutableAttributedString {
        let result = NSMutableAttributedString()
        
        forEach { (element) in
            if element != self.first {
                result += NSAttributedString(string: separator, font: nil)
            }
            result += element
        }
        
        return result
    }
}

extension Sequence where Element : OptionalType, Element.Wrapped : NSAttributedString {
    func concat(separator: String? = nil) -> NSMutableAttributedString {
        let nonNils: [NSAttributedString] = compactMap { $0.value }
        return nonNils.concat(separator: separator)
    }
}
