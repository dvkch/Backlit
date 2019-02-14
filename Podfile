inhibit_all_warnings!

platform :ios, '9.0'

use_frameworks!

target :'SaneScanner' do
    pod 'SaneSwift', :path => "./"

	pod 'SVProgressHUD'
	pod 'NSDate+TimeAgo'
	pod 'SYLogRedirect'
	pod 'DLAlertView'
	pod 'Masonry'
	pod 'PKYStepper'
	pod 'SYKit'
	pod 'SYPair'
	pod 'SpinKit'
	pod 'SYOperationQueue'
	pod 'WeakUniqueCollection'
	pod 'libextobjc/EXTScope'
	pod 'SYPictureMetadata'
	pod 'SYEmailHelper'
	pod 'MHVideoPhotoGallery', :podspec => "https://raw.githubusercontent.com/dvkch/MHVideoPhotoGallery/master/MHVideoPhotoGallery.podspec"
	pod 'MHWDirectoryWatcher', :podspec => "https://raw.githubusercontent.com/dvkch/MHWDirectoryWatcher/master/MHWDirectoryWatcher.podspec"

    pod 'SnapKit'
    pod 'DirectoryWatcher'
end

post_install do | installer |
    require 'fileutils'
    FileUtils.cp_r('Pods/Target Support Files/Pods-SaneScanner/Pods-SaneScanner-acknowledgements.plist', 'SaneScanner/Settings.bundle/Acknowledgements.plist', :remove_destination => true)

    installer.pods_project.targets.each do |t|
        t.build_configurations.each do |config|
            config.build_settings['IPHONEOS_DEPLOYMENT_TARGET'] = '9.0'
            config.build_settings['ENABLE_BITCODE'] = 'NO'
        end
    end
end
