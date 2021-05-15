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
        
        useMacOSThumb = UIDevice.isCatalyst
        
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
        get { return slider.maximumValue }
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
    private var useMacOSThumb: Bool = false {
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
    
    func incrementStep(smallSteps: Bool = false) -> Float {
        if let step = step {
            return step
        }
        return (maximumValue - minimumValue) / (smallSteps ? 100 : 25)
    }

    func increment(smallSteps: Bool = false) {
        guard isEnabled else { return }

        value += incrementStep(smallSteps: smallSteps)
        changedBlock?(value)
    }
    
    func decrement(smallSteps: Bool = false) {
        guard isEnabled else { return }

        value -= incrementStep(smallSteps: smallSteps)
        changedBlock?(value)
    }

    // MARK: Keyboard
    override var keyCommands: [UIKeyCommand]? {
        return [
            UIKeyCommand(input: UIKeyCommand.inputLeftArrow, modifierFlags: .init(), action: #selector(pressedArrow(_:))),
            UIKeyCommand(input: UIKeyCommand.inputRightArrow, modifierFlags: .init(), action: #selector(pressedArrow(_:))),
        ]
    }
    
    @objc func pressedArrow(_ command: UIKeyCommand) {
        guard isEnabled else { return }

        switch command.input {
        case UIKeyCommand.inputLeftArrow: decrement()
        case UIKeyCommand.inputRightArrow: increment()
        default: break
        }
    }

    // MARK: Content
    private func updateContent() {
        if let step = step {
            slider.value = roundf(slider.value / step) * step;
        }

        label.text = formatter?(slider.value) ?? String(slider.value)
    }
}
