# IFMDebugTool

[![Version](http://img.shields.io/cocoapods/v/IFMDebugTool.svg?style=flat)](http://cocoapods.org/?q=IFMDebugTool)
 [![Platform](http://img.shields.io/cocoapods/p/IFMDebugTool.svg?style=flat)]()
 [![License](http://img.shields.io/cocoapods/l/IFMDebugTool.svg?style=flat)](https://github.com/JohnWong/iOS-file-manager/blob/master/LICENSE)

Manage your app's file system in browser.

## Screenshot

![Screenshot](https://raw.githubusercontent.com/JohnWong/iOS-file-manager/master/Docs/screenshot.png)

You can click to expand folder or open/download file. Right click to open or delete.

## Installation

Install via Cocoapods:
```
pod 'IFMDebugTool,:configurations => ['Debug']
```

## Access URL

Three ways to get access URL.

### Printed to console after app is started

Access URL will be print after app is started:
```
2015-09-13 22:31:03.182 iOSFileManager[58434:3139811] IFMDebugTool: http://10.11.243.16:10000
```

### Shown after shake your device

![Screenshot](https://raw.githubusercontent.com/JohnWong/iOS-file-manager/master/Docs/device-screenshot.png)

### Find by Bonjour Service

```
dns-sd -B _ifm._tcp local
```

![Screenshot](https://raw.githubusercontent.com/JohnWong/iOS-file-manager/master/Docs/terminal.png)
