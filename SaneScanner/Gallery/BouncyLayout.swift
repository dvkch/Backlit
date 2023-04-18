//
//  BouncyLayout.swift
//  SaneScanner
//
//  Created by syan on 12/04/2023.
//  Copyright © 2023 Syan. All rights reserved.
//

import UIKit

// https://www.objc.io/issues/5-ios7/collection-views-and-uidynamics/
// https://github.com/roberthein/BouncyLayout/blob/master/BouncyLayout/Classes/BouncyLayout.swift
class BouncyLayout: UICollectionViewFlowLayout {
    
    // MARK: Init
    override init() {
        super.init()
        animator.delegate = self
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: Properties
    var bounciness: CGFloat = 1
    private lazy var animator = UIDynamicAnimator(collectionViewLayout: self)
    private var visibleIdentifiers: [BouncyIdentifier: CGRect] = [:]
    var applyEffectPerLine: Bool = true
    
    // MARK: Animation
    private func updateVisibleIdentifiers() {
        let visibleRect = CGRect(
            origin: collectionView?.bounds.origin ?? .zero,
            size: collectionView?.frame.size ?? .zero
        ).insetBy(dx: -100, dy: -100)

        let oldVisibleIdentifiers = self.visibleIdentifiers
        let visibleItems = super.layoutAttributesForElements(in: visibleRect) ?? []
        visibleIdentifiers = visibleItems.reduce(into: [:]) { $0[$1.bouncyIdentifier] = $1.frame }
        recreateBehaviours(from: oldVisibleIdentifiers, to: visibleIdentifiers, using: visibleItems)
    }

    private func recreateBehaviours(
        from previousIdentifiers: [BouncyIdentifier: CGRect], to updatedIdentifiers: [BouncyIdentifier: CGRect],
        using visibleAttributes: [UICollectionViewLayoutAttributes]
    ) {
        let indexedVisibleAttributes = visibleAttributes.reduce(into: [:]) { $0[$1.bouncyIdentifier] = $1 }
        
        let addedIdentifiers = Set(updatedIdentifiers.keys).subtracting(previousIdentifiers.keys)
        let removedIdentifiers = Set(previousIdentifiers.keys).subtracting(updatedIdentifiers.keys)
        let existingIdentifiers = Set(updatedIdentifiers.keys).intersection(previousIdentifiers.keys)
        let newFrameIdentifiers = existingIdentifiers.filter { previousIdentifiers[$0] != updatedIdentifiers[$0] }

        // remove no longer used or out of date behaviors
        animator.behaviors
            .compactMap { $0 as? BouncyBehavior }
            .filter { removedIdentifiers.contains($0.item.bouncyIdentifier) || newFrameIdentifiers.contains($0.item.bouncyIdentifier) }
            .forEach {
                animator.removeBehavior($0)
            }
        
        // create new behaviors and recreate the ones that were outdated
        let addedBehaviors = addedIdentifiers.union(newFrameIdentifiers)
            .compactMap { indexedVisibleAttributes[$0] }
            .map {
                let behavior = BouncyBehavior(item: $0, onlyInDirection: scrollDirection)
                animator.addBehavior(behavior)
                return behavior
            }

        // set the current value
        updateBehaviors(addedBehaviors)
    }

    private func updateBehaviors(_ behaviors: [BouncyBehavior]) {
        let touchLocation = collectionView?.panGestureRecognizer.location(in: collectionView) ?? .zero
        guard touchLocation != .zero else { return }

        guard userStartedScrolling && !UIAccessibility.isReduceMotionEnabled else {
            behaviors.forEach { animator.updateItem(usingCurrentState: $0.item) }
            return
        }
        
        let applyOnX = scrollDirection == .horizontal || !applyEffectPerLine
        let applyOnY = scrollDirection == .vertical || !applyEffectPerLine

        behaviors.forEach { behavior in
            let xDistanceFromTouch = applyOnX ? abs(touchLocation.x - behavior.anchorPoint.x) : 0
            let yDistanceFromTouch = applyOnY ? abs(touchLocation.y - behavior.anchorPoint.y) : 0

            let scrollResistance = (yDistanceFromTouch + xDistanceFromTouch) / 1500 * bounciness
            
            var center = behavior.item.center
            switch scrollDirection {
            case .horizontal:
                let delta = latestDelta.dx
                if (delta < 0) {
                    center.x += max(delta, delta*scrollResistance)
                }
                else {
                    center.x += min(delta, delta*scrollResistance)
                }
            case .vertical:
                let delta = latestDelta.dy
                if (delta < 0) {
                    center.y += max(delta, delta*scrollResistance)
                }
                else {
                    center.y += min(delta, delta*scrollResistance)
                }
            @unknown default:
                break
            }
            behavior.item.center = center
            animator.updateItem(usingCurrentState: behavior.item)
        }
    }

    // MARK: Subclassing
    override func prepare() {
        super.prepare()
        updateVisibleIdentifiers()
    }
    
    override func prepare(forCollectionViewUpdates updateItems: [UICollectionViewUpdateItem]) {
        super.prepare(forCollectionViewUpdates: updateItems)
        updateVisibleIdentifiers()
    }
    
    override func layoutAttributesForElements(in rect: CGRect) -> [UICollectionViewLayoutAttributes]? {
        return (animator.items(in: rect) as! [UICollectionViewLayoutAttributes])
    }
    
    override func layoutAttributesForItem(at indexPath: IndexPath) -> UICollectionViewLayoutAttributes? {
        return (
            animator.layoutAttributesForCell(at: indexPath) ??
            super.layoutAttributesForItem(at: indexPath)
        )
    }
    
    override func layoutAttributesForDecorationView(ofKind elementKind: String, at indexPath: IndexPath) -> UICollectionViewLayoutAttributes? {
        return (
            animator.layoutAttributesForDecorationView(ofKind: elementKind, at: indexPath) ??
            super.layoutAttributesForDecorationView(ofKind: elementKind, at: indexPath)
        )
    }
    
    override func layoutAttributesForSupplementaryView(ofKind elementKind: String, at indexPath: IndexPath) -> UICollectionViewLayoutAttributes? {
        return (
            animator.layoutAttributesForSupplementaryView(ofKind: elementKind, at: indexPath) ??
            super.layoutAttributesForSupplementaryView(ofKind: elementKind, at: indexPath)
        )
    }
    
    private var userStartedScrolling: Bool = false
    private var latestDelta: CGVector = .zero
    override func shouldInvalidateLayout(forBoundsChange newBounds: CGRect) -> Bool {
        // if at any point during out scroll the user touches the screen, then we use the animator
        // if not, we will have a simple scrolling. this is reset after the animator has ended for the
        // current scroll
        userStartedScrolling = (collectionView?.isTracking ?? false) || userStartedScrolling
        
        let oldBounds = (collectionView?.bounds ?? .zero)
        latestDelta = CGVector(dx: newBounds.origin.x - oldBounds.origin.x, dy: newBounds.origin.y - oldBounds.origin.y)
        updateBehaviors(animator.behaviors.compactMap { $0 as? BouncyBehavior })

        return super.shouldInvalidateLayout(forBoundsChange: newBounds)
    }
}

extension BouncyLayout: UIDynamicAnimatorDelegate {
    func dynamicAnimatorDidPause(_ animator: UIDynamicAnimator) {
        userStartedScrolling = false
    }
}

fileprivate class BouncyBehavior: UIDynamicBehavior {
    init(item: UICollectionViewLayoutAttributes, onlyInDirection: UICollectionView.ScrollDirection) {
        self.item = item
        self.behavior = .init(item: item, attachedToAnchor: item.center)
        super.init()
        
        behavior.length = 0.0
        behavior.damping = 1.2
        behavior.frequency = 1.0
        behavior.action = { [weak self] in
            switch onlyInDirection {
            case .vertical:
                item.center.x = self?.anchorPoint.x ?? 0
            case .horizontal:
                item.center.y = self?.anchorPoint.y ?? 0
            @unknown default:
                break
            }
        }
        addChildBehavior(behavior)
    }
    
    let item: UICollectionViewLayoutAttributes
    private let behavior: UIAttachmentBehavior
    var anchorPoint: CGPoint {
        get {
            behavior.anchorPoint
        }
        set {
            behavior.anchorPoint = newValue
        }
    }
}

struct BouncyIdentifier: Equatable, Hashable, CustomStringConvertible {
    let category: UICollectionView.ElementCategory
    let kind: String?
    let indexPath: IndexPath
    
    static var invalid = BouncyIdentifier(category: .cell, kind: nil, indexPath: IndexPath(item: -1, section: -1))

    var description: String {
        let categoryString: String
        switch category {
        case .cell:                 categoryString = "cell"
        case .supplementaryView:    categoryString = "supplementaryView"
        case .decorationView:       categoryString = "decorationView"
        @unknown default:           categoryString = "unknown-\(category.rawValue)"
        }
        return "\(indexPath) - \(categoryString) - \(kind ?? "<none>")"
    }
}

extension UICollectionViewLayoutAttributes {
    var bouncyIdentifier: BouncyIdentifier {
        return .init(category: representedElementCategory, kind: representedElementKind, indexPath: indexPath)
    }
}

