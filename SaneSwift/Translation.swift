//
//  Translation.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 30/01/19.
//  Copyright (c) 2019 Syan. All rights reserved.
//

public struct Translation {
    
    // MARK: Init
    init(translations: [String: String]) {
        self.translations = translations
    }
    
    init?(contentsOfURL url: URL) {
        guard let translations = Translation.parseFile(at: url) else { return nil }
        self.init(translations: translations)
    }
    
    public init?(locale: Locale) {
        let bundle = Bundle(for: Sane.self)
        guard let translationsBundleURL = bundle.url(forResource: "SaneTranslations", withExtension: "bundle") else { return nil }
        guard let translationsBundle = Bundle(url: translationsBundleURL) else { return nil }
        
        // TODO: cleanup!!
        let filename = "sane_strings_" + (locale.languageCode ?? locale.identifier)
        let availableURLs = translationsBundle.urls(forResourcesWithExtension: "po", subdirectory: nil) ?? []
        guard let translationURL = availableURLs.first(where: { $0.lastPathComponent.hasPrefix(filename) }) else { return nil }
        
        self.init(contentsOfURL: translationURL)
    }

    // MARK: Properties
    private let translations: [String: String]
    
    public func translation(for key: String) -> String? {
        return translations[key]
    }
}

extension Translation {
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
                    
                    if !value.isEmpty {
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


