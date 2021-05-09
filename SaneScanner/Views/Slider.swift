//
//  Slider.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 08/05/2021.
//  Copyright © 2021 Syan. All rights reserved.
//

import UIKit

class Slider: UIControl {

    // MARK: Init
    override init(frame: CGRect) {
        super.init(frame: frame)
        setup()
    }
    
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        setup()
    }
    
    private func setup() {
        container.axis = .horizontal
        container.distribution = .fill
        container.spacing = 10
        container.setContentHuggingPriority(.required, for: .horizontal)
        container.setContentHuggingPriority(.required, for: .vertical)
        addSubview(container)
        container.snp.makeConstraints { make in
            make.edges.equalTo(self)
        }
        
        slider.addTarget(self, action: #selector(sliderChanged(_:event:)), for: .valueChanged)
        container.addArrangedSubview(slider)

        label.textColor = .normalText
        label.font = .preferredFont(forTextStyle: .body)
        label.autoAdjustsFontSize = true
        label.isUserInteractionEnabled = true
        container.addArrangedSubview(label)
        
        let doubleTapGesture = UITapGestureRecognizer(target: self, action: #selector(doubleTapGestureRecognized))
        doubleTapGesture.numberOfTapsRequired = 2
        label.addGestureRecognizer(doubleTapGesture)
    }

    // MARK: Properties
    var value: Float {
        get { return slider.value }
        set { slider.value = newValue; updateContent() }
    }
    var minimumValue: Float {
        get { return slider.minimumValue }
        set { slider.minimumValue = newValue; updateContent() }
    }
    var maximumValue: Float {
        get { return slider.minimumValue }
        set { slider.maximumValue = newValue; updateContent() }
    }
    var step: Float? {
        didSet {
            updateContent()
        }
    }
    override var isEnabled: Bool {
        didSet {
            slider.isEnabled = isEnabled
            label.isEnabled = isEnabled
        }
    }
    var formatter: ((Float) -> String)? {
        didSet {
            updateContent()
        }
    }
    var useMacOSThumb: Bool = false {
        didSet {
            if !useMacOSThumb {
                slider.setThumbImage(nil, for: .normal)
                return
            }

            let size = CGSize(width: 10, height: 20)
            let image = UIImage.screenshottingContext(size: size) {
                UIColor.normalText.setFill()
                UIBezierPath(roundedRect: .init(origin: .zero, size: size), cornerRadius: 4).fill()
            }
            slider.setThumbImage(image, for: .normal)
        }
    }
    var changedBlock: ((Float) -> ())?
    var doubleTapBlock: (() -> ())?
    
    // MARK: Views
    private let container = UIStackView()
    private let slider = UISlider()
    private let label = UILabel()
    
    // MARK: Actions
    @objc private func sliderChanged(_ slider: UISlider, event: UIEvent) {
        updateContent()

        if event.allTouches?.first?.phase == .ended {
            changedBlock?(slider.value)
        }
    }
    
    @objc private func doubleTapGestureRecognized() {
        doubleTapBlock?()
    }

    // MARK: Content
    private func updateContent() {
        if let step = step {
            slider.value = roundf(slider.value / step) * step;
        }

        label.text = formatter?(slider.value) ?? String(slider.value)
    }
}
