//
//  Translation.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

import Foundation

// MARK: Public access
public extension String {
    var saneTranslation: String {
        return libSANETranslation.translation(for: self) ?? SaneSwiftTranslation.translation(for: self) ?? self
    }
}

// MARK: SaneSwift translations
private struct SaneSwiftTranslation {
    static private var cachedBundle: Bundle?
    
    fileprivate static func translation(for key: String) -> String? {
        if cachedBundle == nil {
            guard let translationsBundleURL = Bundle(for: Sane.self).url(forResource: "SaneSwift-Translations", withExtension: "bundle") else { return nil }
            cachedBundle = Bundle(url: translationsBundleURL)
        }
        
        return cachedBundle?.localizedString(forKey: key, value: nil, table: nil)
    }
}

// MARK: libSANE translations
private struct libSANETranslation {
    static private var cachedTranslations: [String: String]?

    fileprivate static func translation(for key: String) -> String? {
        if cachedTranslations == nil {
            guard let translationsBundleURL = Bundle(for: Sane.self).url(forResource: "Sane-Translations", withExtension: "bundle") else { return nil }
            guard let translationsBundle = Bundle(url: translationsBundleURL) else { return nil }
            guard let translationURL = translationsBundle.url(forResource: "sane_strings", withExtension: "po") else { return nil }
            cachedTranslations = libSANETranslation.parseFile(at: translationURL)
        }
        
        return cachedTranslations?[key]
    }
}

extension libSANETranslation {
    // MARK: Parsing
    private static func parseFile(at url: URL) -> [String: String]? {
        guard let content = try? String(contentsOf: url) else { return nil }
        
        let keyToken = "msgid \""
        let valueToken = "msgstr \""
        let multilineToken = "\""
        
        // keep only lines that start with:
        // - "
        // - msgid "
        // - msgstr "
        let lines = content
            .split(separator: "\n")
            .filter { $0.hasPrefix(multilineToken) || $0.hasPrefix(keyToken) || $0.hasPrefix(valueToken) }
        
        // strings can be on multiple lines, here we merge them.
        // we read line by line, and merge the next line as long as it starts
        // with a ", then remove all occurences of "".
        let mergedLines = lines.reduce(into: []) { ( result: inout [Substring], element: Substring) in
            if !result.isEmpty && element.hasPrefix(multilineToken) {
                result[result.count - 1] = result[result.count - 1].dropLast() + element.dropFirst()
            }
            else {
                result.append(element)
            }
        }
        
        // read content line by line, save key and value as they come, each
        // time we have a value we save it if its key is valid
        //var values = [String: String](minimumCapacity: mergedLines / 2 + 1)
        var translations = [String: String]()
        
        var currentKey: String? = nil
        for line in mergedLines {
            if line.hasPrefix(keyToken) {
                currentKey = line
                    .dropFirst(keyToken.count).dropLast()
                    .replacingOccurrences(of: "\\\"", with: "\"")
                continue
            }
            
            if line.hasPrefix(valueToken) {
                if let key = currentKey {
                    let value = line
                        .dropFirst(valueToken.count).dropLast()
                        .replacingOccurrences(of: "\\\"", with: "\"")
                        .trimmingCharacters(in: .whitespacesAndNewlines)
                    
                    if !key.isEmpty && !value.isEmpty {
                        translations[key] = value
                    }
                    currentKey = nil
                }
                else {
                    print("Found value before knowing the key. Line:", line)
                }
                continue
            }
            
            print("Invalid line found:", line)
        }
        
        return translations
    }
}


