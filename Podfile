inhibit_all_warnings!

platform :ios, '12.0'

use_frameworks!

target :Backlit do
    pod 'Sane', :path => "./", :inhibit_warnings => false
    pod 'SaneSwift', :path => "./", :inhibit_warnings => false
    pod 'DiffableDataSources'
    pod 'DirectoryWatcher'
    pod 'KeychainAccess'
    pod 'LRUCache'
    pod 'SnapKit'
    pod 'SpinKit'
    pod 'SYEmailHelper'
    pod 'SYKit', '>= 0.1.17'
    pod 'SYOperationQueue'
    pod 'SYPictureMetadata', '~> 2.0'
    pod 'TelemetryClient', :podspec => "https://raw.githubusercontent.com/TelemetryDeck/SwiftClient/main/TelemetryClient.podspec"
end

post_install do |installer|
    require 'fileutils'
    FileUtils.cp_r('Pods/Target Support Files/Pods-Backlit/Pods-Backlit-acknowledgements.plist', 'Backlit/Settings.bundle/Acknowledgements.plist', :remove_destination => true)

    installer.pods_project.targets.each do |target|
        is_bundle = target.respond_to?(:product_type) && target.product_type == "com.apple.product-type.bundle"

        target.build_configurations.each do |config|
            config.build_settings['IPHONEOS_DEPLOYMENT_TARGET'] = '12.0'
            config.build_settings['ENABLE_BITCODE'] = 'NO'

            # Fix bundle targets' 'Signing Certificate' to 'Sign to Run Locally'
            # https://github.com/CocoaPods/CocoaPods/issues/8891#issuecomment-573301570
            config.build_settings['CODE_SIGN_IDENTITY[sdk=macosx*]'] = '-' if is_bundle

            # Fix bundle targets' not signed on iOS
            config.build_settings['DEVELOPMENT_TEAM'] = '79RY8264V4' if is_bundle
        end
    end
end
