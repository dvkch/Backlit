inhibit_all_warnings!

platform :ios, '9.0'

use_frameworks!

target :'SaneScanner' do
    pod 'SaneSwift', :path => "./", :inhibit_warnings => false
    pod 'DirectoryWatcher'
    pod 'SnapKit'
    pod 'SpinKit'
    pod 'SVProgressHUD', :configurations => ['Debug', 'Release']
    pod 'SYEmailHelper', :configurations => ['Debug', 'Release']
    pod 'SYKit'
    pod 'SYOperationQueue'
    pod 'SYPictureMetadata', :configurations => ['Debug', 'Release'], :inhibit_warnings => false
end

post_install do | installer |
    require 'fileutils'
    FileUtils.cp_r('Pods/Target Support Files/Pods-SaneScanner/Pods-SaneScanner-acknowledgements.plist', 'SaneScanner/Settings.bundle/Acknowledgements.plist', :remove_destination => true)

    installer.pods_project.targets.each do |t|
        t.build_configurations.each do |config|
            if config.name == "macOS"
                config.build_settings['IPHONEOS_DEPLOYMENT_TARGET'] = '12.0'
            else
                config.build_settings['IPHONEOS_DEPLOYMENT_TARGET'] = '9.0'
            end

            config.build_settings['ENABLE_BITCODE'] = 'NO'
        end
    end
end
