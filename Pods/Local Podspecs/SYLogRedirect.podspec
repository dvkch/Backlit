Pod::Spec.new do |s|
  s.ios.deployment_target = '5.0'
  s.osx.deployment_target = '10.7'
  s.name     = 'SYLogRedirect'
  s.version  = '1.2.2'
  s.license  = 'Custom'
  s.summary  = 'Log redirect tool'
  s.homepage = 'https://github.com/dvkch/SYLogRedirect'
  s.author   = { 'Stan Chevallier' => 'contact@stanislaschevallier.fr' }
  s.source   = { :git => 'https://github.com/dvkch/SYLogRedirect.git', :tag => s.version.to_s }
  s.source_files = 'SYLogRedirect.{h,m}'
  s.requires_arc = true
  s.xcconfig = { 'CLANG_MODULES_AUTOLINK' => 'YES' }
end
