Pod::Spec.new do |s|
  s.ios.deployment_target  = '8.0'
  s.name     = 'SaneSwift'
  s.version  = '1.0.0'
  s.license  = 'Custom'
  s.summary  = 'Swift wrapper for SANE backends'
  s.homepage = 'https://github.com/dvkch/SaneScanner'
  s.author   = { 'Stan Chevallier' => 'contact@stanislaschevallier.fr' }
  s.source   = { :git => 'https://github.com/dvkch/SaneScanner.git', :tag => s.version.to_s }
  s.swift_version = "4.2"
  
  s.source_files = 'SaneSwift/*.{h,c,m,swift}', 'sane-libs/all/*.h'
  s.vendored_libraries = 'sane-libs/all/**/libsane-net.a'
  s.resource_bundles = { 
    'libSANE-Translations' => ['sane-libs/all/translations/*.lproj']
  }

  s.requires_arc = true
  s.xcconfig = { 'CLANG_MODULES_AUTOLINK' => 'YES' }
end
