//
//  Acknowledgement.swift
//  SaneScanner
//
//  Created by syan on 19/03/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

private struct AcknowledgementsWrapper: Codable {
    let acknowledgements: [Acknowledgement]
    
    private enum CodingKeys: String, CodingKey {
        case acknowledgements = "PreferenceSpecifiers"
    }
}

struct Acknowledgement: Codable {

    // MARK: Properties
    let title: String
    let licenseName: String!
    let licenseText: String
    
    private enum CodingKeys: String, CodingKey {
        case title = "Title"
        case licenseName = "License"
        case licenseText = "FooterText"
    }

    // MARK: Parsing
    static private(set) var acknowledgementsURL = Bundle.main.url(
        forResource: "Acknowledgements",
        withExtension: "plist",
        subdirectory: "Settings.bundle"
    )

    static var parsed: [Acknowledgement] = {
        guard let acknowledgementsURL else { return [] }
        do {
            let data = try Data(contentsOf: acknowledgementsURL)
            let decoder = PropertyListDecoder()
            let wrapper = try PropertyListDecoder().decode(AcknowledgementsWrapper.self, from: data)
            return wrapper.acknowledgements.filter { $0.licenseName != nil }
        }
        catch {
            print("Couldn't parse acknowledgements:", error)
            return []
        }
    }()
}
