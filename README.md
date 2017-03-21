# SaneScanner

## What is it?

An iOS app connecting to your [SANE](http://www.sane-project.org/) server, using the appropriate `net` backend, to scan documents from your iOS device!

To make it work:

- add your server IP address or hostname,
- tap your device once it's found
- scan!

Scanned documents can be shared as a multi-page PDF or as individual images via the iOS share dialog.

## Screenshots

<img src="screenshots/en-US/iPhone6-01-Devices-d41d8cd98f00b204e9800998ecf8427e.png?raw=true" alt="Devices" width="240"/>
<img src="screenshots/en-US/iPhone6-02-DeviceWithPreview-d41d8cd98f00b204e9800998ecf8427e.png?raw=true" alt="Devices" style="width: 240px;"/>
<img src="screenshots/en-US/iPhone6-03-DeviceWithOptions-d41d8cd98f00b204e9800998ecf8427e.png?raw=true" alt="Devices" style="width: 240px;"/>
<img src="screenshots/en-US/iPhone6-04-DeviceWithOptionPopup-d41d8cd98f00b204e9800998ecf8427e.png?raw=true" alt="Devices" style="width: 240px;"/>
<img src="screenshots/en-US/iPhone6-05-Settings-d41d8cd98f00b204e9800998ecf8427e.png?raw=true" alt="Devices" style="width: 240px;"/>

## Useful links

The following links have helped me a great deal in the making of this app, if you're building a desktop lib for iOS you may find them useful to!

##### CrossCompiling Sane

- https://github.com/tpoechtrager/cctools-port/issues/6

- https://github.com/obfuscator-llvm/obfuscator/issues/13

- https://help.ubuntu.com/community/CompileSaneFromSource

- http://stackoverflow.com/questions/26812060/cross-compile-libgcrypt-static-lib-for-use-on-ios

- https://ghc.haskell.org/trac/ghc/wiki/Building/CrossCompiling/iOS


##### Sane API doc

- http://www.sane-project.org/html/doc012.html

- http://www.sane-project.org/html/doc011.html#s4.2.9.7


##### Other Sane projects

- https://github.com/chrspeich/SaneNetScanner

