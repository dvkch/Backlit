//
//  GalleryGroup.swift
//  Backlit
//
//  Created by syan on 16/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import Foundation

struct GalleryGroup {
    
    // MARK: Init
    private init(items: [GalleryItem], date: Date) {
        self.items = items
        self.date = date
    }

    init?(items: [GalleryItem]) {
        guard let firstItem = items.first else {
            return nil
        }
        self.items = items
        self.date = Calendar.current.startOfDay(for: firstItem.creationDate)
    }
    
    static func stable() -> GalleryGroup {
        return GalleryGroup(items: [], date: Date(timeIntervalSince1970: 0))
    }
    
    static func grouping(_ items: [GalleryItem]) -> [GalleryGroup] {
        guard let firstItem = items.first else {
            return []
        }
        
        var groups = [[GalleryItem]]()
        groups.append([firstItem])
        
        let calendar = Calendar.autoupdatingCurrent
        for item in items.dropFirst() {
            let prevItem = groups.last!.last!
            if calendar.isDate(item.creationDate, inSameDayAs: prevItem.creationDate) ||
                item.creationDate.timeIntervalSince(prevItem.creationDate) < 3600
            {
                groups[groups.count - 1].append(item)
            }
            else {
                groups.append([item])
            }
        }
        
        return groups.compactMap { GalleryGroup(items: $0) }
    }
    
    // MARK: Properties
    let items: [GalleryItem]
    let date: Date
}

extension GalleryGroup: Hashable {
    static func ==(lhs: GalleryGroup, rhs: GalleryGroup) -> Bool {
        return lhs.date == rhs.date
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(date)
    }
}
