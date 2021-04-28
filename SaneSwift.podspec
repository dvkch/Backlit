Pod::Spec.new do |s|
  s.ios.deployment_target  = '9.0'
  s.name     = 'SaneSwift'
  s.version  = '1.0.0'
  s.license  = 'MIT'
  s.summary  = 'Swift wrapper for SANE backends'
  s.homepage = 'https://github.com/dvkch/SaneScanner'
  s.author   = { 'Stan Chevallier' => 'contact@stanislaschevallier.fr' }
  s.source   = { :git => 'https://github.com/dvkch/SaneScanner.git', :tag => 'SaneSwift-' + s.version.to_s }
  s.swift_version = '5.0'
  
  s.source_files = 'SaneSwift/*.{h,c,m,swift}'
  s.resource_bundles = { 
    'SaneSwift-Translations' => ['SaneSwift/Localizable/*.lproj']
  }

  s.requires_arc = true
  s.xcconfig = { 'CLANG_MODULES_AUTOLINK' => 'YES' }
  s.dependency 'SYPictureMetadata', '~> 2.0'
  s.dependency 'Sane'

  s.static_framework = true
end
