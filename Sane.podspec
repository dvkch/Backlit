Pod::Spec.new do |s|
  s.ios.deployment_target  = '9.0'
  s.name     = 'Sane'
  s.version  = '1.0.31'
  s.license  = { :type => 'GPL with exception clause', :file => 'Sane/all/Sane.xcframework/LICENSE.txt' }
  s.summary  = 'SANE backends, net only'
  s.homepage = 'https://github.com/dvkch/SaneScanner'
  s.author   = { 'Stan Chevallier' => 'contact@stanislaschevallier.fr' }
  s.source   = { :git => 'https://github.com/dvkch/SaneScanner.git', :tag => 'Sane-' + s.version.to_s }
  
  s.vendored_frameworks = 'Sane/all/Sane.xcframework'
  s.resource_bundles = { 
    'Sane-Translations' => ['Sane/all/translations/*.lproj']
  }

  s.requires_arc = true
  s.xcconfig = { 'CLANG_MODULES_AUTOLINK' => 'YES' }
end
