inhibit_all_warnings!

platform :ios, '8.0'

target :'SaneScanner' do
	pod 'SVProgressHUD'
	pod 'NSDate+TimeAgo'
	pod 'SYLogRedirect'
	pod 'DLAlertView'
	pod 'Masonry'
	pod 'PKYStepper'
	pod 'SYKit'
	pod 'SYPair'
	pod 'INSPullToRefresh'
	pod 'SpinKit'
	pod 'SYOperationQueue'
	pod 'WeakUniqueCollection'
	pod 'libextobjc/EXTScope'
	pod 'SYPictureMetadata'
	pod 'SYEmailHelper'
	pod 'HockeySDK'
	pod 'MHVideoPhotoGallery', :podspec => "https://raw.githubusercontent.com/dvkch/MHVideoPhotoGallery/master/MHVideoPhotoGallery.podspec"
	pod 'MHWDirectoryWatcher', :podspec => "https://raw.githubusercontent.com/dvkch/MHWDirectoryWatcher/master/MHWDirectoryWatcher.podspec"
end

post_install do | installer |
  require 'fileutils'
  FileUtils.cp_r('Pods/Target Support Files/Pods-SaneScanner/Pods-SaneScanner-acknowledgements.plist', 'SaneScanner/Settings.bundle/Acknowledgements.plist', :remove_destination => true)
end
