//
//  DeviceOptionVC.swift
//  SaneScanner
//
//  Created by Stanislas Chevallier on 03/03/2019.
//  Copyright © 2019 Syan. All rights reserved.
//

import UIKit
import SaneSwift
import SVProgressHUD

class DeviceOptionVC: UIViewController {

    convenience init() {
        self.init(nibName: nil, bundle: nil)
        self.modalPresentationStyle = .popover
        self.popoverPresentationController?.delegate = self
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        updateContent()
        
        setAutoButton.setTitle("OPTION VALUE AUTO".localized, for: .normal)
        setValueButton.setTitle("ACTION SET VALUE".localized, for: .normal)
        pressButton.setTitle("ACTION PRESS".localized, for: .normal)
        closeButton.setTitle("ACTION CLOSE".localized, for: .normal)
        
        contentView.addObserver(self, forKeyPath: #keyPath(UIScrollView.contentSize), options: .new, context: nil)
    }
    
    deinit {
        contentView.removeObserver(self, forKeyPath: #keyPath(UIScrollView.contentSize))
    }
    
    override func observeValue(forKeyPath keyPath: String?, of object: Any?, change: [NSKeyValueChangeKey : Any]?, context: UnsafeMutableRawPointer?) {
        if keyPath == #keyPath(UIScrollView.contentSize) {
            updateContentViewHeight()
            return
        }
        super.observeValue(forKeyPath: keyPath, of: object, change: change, context: context)
    }

    // MARK: Properties
    var option: DeviceOption?
    var closeBlock: ((Error?) -> ())?
    
    // MARK: Blocks
    private var sliderChangedBlock: ((Float) -> ())?
    private var stepperChangedBlock: ((Double) -> ())?

    // MARK: Views
    @IBOutlet private var titleLabel: UILabel!
    @IBOutlet private var descriptionLabel: UILabel!
    @IBOutlet private var contentView: UIScrollView!
    @IBOutlet private var contentViewHeight: NSLayoutConstraint!
    @IBOutlet private var stackView: UIStackView!
    @IBOutlet private var stepView: UIView!
    @IBOutlet private var stepLabel: UILabel!
    @IBOutlet private var stepper: UIStepper!
    @IBOutlet private var sliderView: UIView!
    @IBOutlet private var slider: UISlider!
    @IBOutlet private var textField: UITextField!
    @IBOutlet private var setValueButton: Button!
    @IBOutlet private var setAutoButton: Button!
    @IBOutlet private var pressButton: Button!
    @IBOutlet private var closeButton: UIButton!
    
    // MARK: Actions
    @IBAction private func stepperChanged() {
        stepperChangedBlock?(stepper.value)
    }
    @IBAction private func sliderChanged() {
        sliderChangedBlock?(slider.value)
    }
    @IBAction private func closeButtonTap() {
        updateCompletion(nil)
    }
    
    private func updateCompletion(_ error: Error?) {
        SVProgressHUD.dismiss()
        dismiss(animated: true, completion: nil)
        closeBlock?(error)
    }
    
    // MARK: Layout
    private func updateContentViewHeight() {
        let minViews: [UIView] = [stepView, sliderView, setValueButton, textField, pressButton, setAutoButton, closeButton]
        let minHeight = minViews.map { $0.bounds.height }.reduce(0, +)
        let maxHeight = presentingViewController?.view.bounds.height ?? view.bounds.height
        contentViewHeight.constant = min(maxHeight, max(minHeight, contentView.contentSize.height))
    }
}

extension DeviceOptionVC : UITextFieldDelegate {
    func textFieldShouldReturn(_ textField: UITextField) -> Bool {
        textField.resignFirstResponder()
        return false
    }
}

extension DeviceOptionVC : UIPopoverPresentationControllerDelegate {
    func adaptivePresentationStyle(for controller: UIPresentationController) -> UIModalPresentationStyle {
        return .none
    }
}

// MARK: Content
extension DeviceOptionVC {
    private func updateContent() {
        guard let option = option else { return }
        
        titleLabel.text = option.localizedTitle
        descriptionLabel.text = option.localizedDescr

        if option.disabledOrReadOnly || option.hasSingleOption {
            stackView.arrangedSubviews.forEach { $0.isHidden = true }
            closeButton.isHidden = false
            return
        }
        
        if let option = option as? DeviceOptionBool {
            updateForValueList(for: option, values: [true, false])
        }
        else if let option = option as? DeviceOptionInt {
            if case .range(let min, let max) = option.constraint {
                updateForRange(for: option, current: Double(option.value), min: Double(min), max: Double(max), step: nil)
            }
            else if case .stepRange(let min, let max, let step, _) = option.constraint {
                updateForRange(for: option, current: Double(option.value), min: Double(min), max: Double(max), step: Double(step))
            }
            else if case let .list(values) = option.constraint {
                updateForValueList(for: option, values: values)
            }
            else {
                fatalError("Int option should have a constraint!")
            }
        }
        else if let option = option as? DeviceOptionFixed {
            if case .range(let min, let max) = option.constraint {
                updateForRange(for: option, current: option.value, min: Double(min), max: Double(max), step: nil)
            }
            else if case .stepRange(let min, let max, let step, _) = option.constraint {
                updateForRange(for: option, current: option.value, min: Double(min), max: Double(max), step: Double(step))
            }
            else if case let .list(values) = option.constraint {
                updateForValueList(for: option, values: values)
            }
            else {
                fatalError("Int option should have a constraint!")
            }
        }
        else if let option = option as? DeviceOptionString {
            if case let .list(values) = option.constraint {
                updateForValueList(for: option, values: values)
            }
            else {
                updateForTextInput(option)
            }
        }
        else if let option = option as? DeviceOptionButton {
            updateForButton(option)
        }
        else if option is DeviceOptionGroup {
        }
        else {
            fatalError("ALL OPTION TYPES SHOULD ALREADY BE HANDLED")
        }
    }
    
    private func updateForValueList<V, T: DeviceOptionTyped<V>>(for option: T, values: [T.Value]) {
        setAutoButton.isHidden = !option.capabilities.contains(.automatic)
        setAutoButton.tapBlock = {
            SVProgressHUD.show()
            Sane.shared.updateOption(option, with: .auto, completion: self.updateCompletion(_:))
        }
        
        for value in values {
            let button = Button(type: .system)
            button.setTitle(option.stringForValue(value, userFacing: true), for: .normal)
            button.tapBlock = {
                SVProgressHUD.show()
                Sane.shared.updateOption(option, with: .value(value), completion: self.updateCompletion(_:))
            }
            
            let closeIndex = stackView.arrangedSubviews.firstIndex(of: closeButton)!
            stackView.insertArrangedSubview(button, at: closeIndex)
        }
    }

    private func updateForRange<V, T: DeviceOptionTyped<V>>(for option: T, current: Double, min: Double, max: Double, step: Double?) {
        let optionInt = option as? DeviceOptionInt
        let optionFixed = option as? DeviceOptionFixed
        
        guard optionInt != nil || optionFixed != nil else {
            fatalError("This method should only be used with numeric options")
        }
        
        setAutoButton.isHidden = !option.capabilities.contains(.automatic)
        setAutoButton.tapBlock = {
            SVProgressHUD.show()
            Sane.shared.updateOption(option, with: .auto, completion: self.updateCompletion(_:))
        }
        
        if let step = step {
            stepView.isHidden = false
            stepLabel.text = option.stringForValue(option.value, userFacing: true)
            stepper.minimumValue = min
            stepper.maximumValue = max
            stepper.stepValue = step
            stepper.value = current
            stepperChangedBlock = { value in
                let stringValue = optionInt?.stringForValue(Int(value), userFacing: true) ?? optionFixed?.stringForValue(Double(value), userFacing: true) ?? ""
                self.stepLabel.text = stringValue
            }
        }
        else {
            sliderView.isHidden = false
            slider.minimumValue = Float(min)
            slider.maximumValue = Float(max)
            slider.value = Float(current)
            sliderChangedBlock = { value in
                let stringValue = optionInt?.stringForValue(Int(value), userFacing: true) ?? optionFixed?.stringForValue(Double(value), userFacing: true) ?? ""
                let buttonTitle = String(format: "ACTION SET VALUE TO %@".localized, stringValue)
                self.setValueButton.setTitle(buttonTitle, for: .normal)
            }
            sliderChangedBlock?(Float(current))
        }
        
        setValueButton.tapBlock = {
            SVProgressHUD.show()
            let value = step != nil ? self.stepper.value : Double(self.slider.value)
            if let optionInt = optionInt {
                Sane.shared.updateOption(optionInt, with: .value(Int(value)), completion: self.updateCompletion(_:))
            }
            if let optionFixed = optionFixed {
                Sane.shared.updateOption(optionFixed, with: .value(value), completion: self.updateCompletion(_:))
            }
        }

    }
    
    private func updateForButton(_ option: DeviceOptionButton) {
        pressButton.isHidden = false
        pressButton.tapBlock = {
            SVProgressHUD.show()
            option.press(self.updateCompletion(_:))
        }
    }
    
    private func updateForTextInput(_ option: DeviceOptionString) {
        setAutoButton.isHidden = !option.capabilities.contains(.automatic)
        setAutoButton.tapBlock = {
            SVProgressHUD.show()
            Sane.shared.updateOption(option, with: .auto, completion: self.updateCompletion(_:))
        }
        
        textField.isHidden = false
        textField.text = option.value
        setValueButton.tapBlock = {
            SVProgressHUD.show()
            Sane.shared.updateOption(option, with: .value(self.textField.text ?? ""), completion: self.updateCompletion(_:))
        }
    }
}
