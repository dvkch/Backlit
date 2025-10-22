// swiftlint:disable all
// Generated using SwiftGen — https://github.com/SwiftGen/SwiftGen

import Foundation

// swiftlint:disable superfluous_disable_command file_length implicit_return prefer_self_in_static_references

// MARK: - Strings

// swiftlint:disable explicit_type_interface function_parameter_count identifier_name line_length
// swiftlint:disable nesting type_body_length type_name vertical_whitespace_opening_braces
internal enum L10n {
  /// Add
  internal static let actionAdd = L10n.tr("Localizable", "ACTION ADD", fallback: "Add")
  /// Don't ask again
  internal static let actionAnalyticsNo = L10n.tr("Localizable", "ACTION ANALYTICS NO", fallback: "Don't ask again")
  /// Allow analytics
  internal static let actionAnalyticsYes = L10n.tr("Localizable", "ACTION ANALYTICS YES", fallback: "Allow analytics")
  /// Localizable.strings
  ///   Backlit
  /// 
  ///   Created by syan on 28/01/2017.
  ///   Copyright © 2017 Syan. All rights reserved.
  internal static let actionCancel = L10n.tr("Localizable", "ACTION CANCEL", fallback: "Cancel")
  /// Close
  internal static let actionClose = L10n.tr("Localizable", "ACTION CLOSE", fallback: "Close")
  /// Continue
  internal static let actionContinue = L10n.tr("Localizable", "ACTION CONTINUE", fallback: "Continue")
  /// Remember
  internal static let actionContinueRemember = L10n.tr("Localizable", "ACTION CONTINUE REMEMBER", fallback: "Remember")
  /// Copy
  internal static let actionCopy = L10n.tr("Localizable", "ACTION COPY", fallback: "Copy")
  /// Delete
  internal static let actionDelete = L10n.tr("Localizable", "ACTION DELETE", fallback: "Delete")
  /// Edit
  internal static let actionEdit = L10n.tr("Localizable", "ACTION EDIT", fallback: "Edit")
  /// Tap to cancel
  internal static let actionHintTapToCancel = L10n.tr("Localizable", "ACTION HINT TAP TO CANCEL", fallback: "Tap to cancel")
  /// Open
  internal static let actionOpen = L10n.tr("Localizable", "ACTION OPEN", fallback: "Open")
  /// Press
  internal static let actionPress = L10n.tr("Localizable", "ACTION PRESS", fallback: "Press")
  /// Refresh
  internal static let actionRefresh = L10n.tr("Localizable", "ACTION REFRESH", fallback: "Refresh")
  /// Remove
  internal static let actionRemove = L10n.tr("Localizable", "ACTION REMOVE", fallback: "Remove")
  /// Save
  internal static let actionSave = L10n.tr("Localizable", "ACTION SAVE", fallback: "Save")
  /// Save to Photos
  internal static let actionSaveToPhotos = L10n.tr("Localizable", "ACTION SAVE TO PHOTOS", fallback: "Save to Photos")
  /// Scan
  internal static let actionScan = L10n.tr("Localizable", "ACTION SCAN", fallback: "Scan")
  /// Update value
  internal static let actionSetValue = L10n.tr("Localizable", "ACTION SET VALUE", fallback: "Update value")
  /// Share
  internal static let actionShare = L10n.tr("Localizable", "ACTION SHARE", fallback: "Share")
  /// Stop scanning
  internal static let actionStopScanning = L10n.tr("Localizable", "ACTION STOP SCANNING", fallback: "Stop scanning")
  /// Do you want to enable anonymous app analytics?
  internal static let analyticsAlertTitle = L10n.tr("Localizable", "ANALYTICS ALERT TITLE", fallback: "Do you want to enable anonymous app analytics?")
  /// Cancelling...
  internal static let cancelling = L10n.tr("Localizable", "CANCELLING", fallback: "Cancelling...")
  /// The current scan will be canceled
  internal static let closeDeviceConfirmationMessage = L10n.tr("Localizable", "CLOSE DEVICE CONFIRMATION MESSAGE", fallback: "The current scan will be canceled")
  /// Are you sure you want to close this page?
  internal static let closeDeviceConfirmationTitle = L10n.tr("Localizable", "CLOSE DEVICE CONFIRMATION TITLE", fallback: "Are you sure you want to close this page?")
  /// contact@syan.me
  internal static let contactAddress = L10n.tr("Localizable", "CONTACT ADDRESS", fallback: "contact@syan.me")
  /// About %@ %@
  internal static func contactSubjectAboutApp(_ p1: Any, _ p2: Any) -> String {
    return L10n.tr("Localizable", "CONTACT SUBJECT ABOUT APP", String(describing: p1), String(describing: p2), fallback: "About %@ %@")
  }
  /// Update preview
  internal static let deviceButtonUpdatePreview = L10n.tr("Localizable", "DEVICE BUTTON UPDATE PREVIEW", fallback: "Update preview")
  /// Preview
  internal static let deviceSectionPreview = L10n.tr("Localizable", "DEVICE SECTION PREVIEW", fallback: "Preview")
  /// Add a new host...
  internal static let devicesRowAddHost = L10n.tr("Localizable", "DEVICES ROW ADD HOST", fallback: "Add a new host...")
  /// Devices
  internal static let devicesSectionDevices = L10n.tr("Localizable", "DEVICES SECTION DEVICES", fallback: "Devices")
  /// Hosts
  internal static let devicesSectionHosts = L10n.tr("Localizable", "DEVICES SECTION HOSTS", fallback: "Hosts")
  /// Enter the hostname or IP address for the new host
  internal static let dialogAddHostMessage = L10n.tr("Localizable", "DIALOG ADD HOST MESSAGE", fallback: "Enter the hostname or IP address for the new host")
  /// Host
  internal static let dialogAddHostPlaceholderHost = L10n.tr("Localizable", "DIALOG ADD HOST PLACEHOLDER HOST", fallback: "Host")
  /// Nickname
  internal static let dialogAddHostPlaceholderName = L10n.tr("Localizable", "DIALOG ADD HOST PLACEHOLDER NAME", fallback: "Nickname")
  /// Add a host
  internal static let dialogAddHostTitle = L10n.tr("Localizable", "DIALOG ADD HOST TITLE", fallback: "Add a host")
  /// Please enter the username and password for %@
  internal static func dialogAuthMessage(_ p1: Any) -> String {
    return L10n.tr("Localizable", "DIALOG AUTH MESSAGE %@", String(describing: p1), fallback: "Please enter the username and password for %@")
  }
  /// Password
  internal static let dialogAuthPlaceholderPassword = L10n.tr("Localizable", "DIALOG AUTH PLACEHOLDER PASSWORD", fallback: "Password")
  /// Username
  internal static let dialogAuthPlaceholderUsername = L10n.tr("Localizable", "DIALOG AUTH PLACEHOLDER USERNAME", fallback: "Username")
  /// Authentication needed
  internal static let dialogAuthTitle = L10n.tr("Localizable", "DIALOG AUTH TITLE", fallback: "Authentication needed")
  /// Couldn't open device
  internal static let dialogCouldntOpenDeviceTitle = L10n.tr("Localizable", "DIALOG COULDNT OPEN DEVICE TITLE", fallback: "Couldn't open device")
  /// Confirmation
  internal static let dialogDeleteScansTitle = L10n.tr("Localizable", "DIALOG DELETE SCANS TITLE", fallback: "Confirmation")
  /// Edit the host
  internal static let dialogEditHostTitle = L10n.tr("Localizable", "DIALOG EDIT HOST TITLE", fallback: "Edit the host")
  /// Memorize this host
  internal static let dialogPersistHostTitle = L10n.tr("Localizable", "DIALOG PERSIST HOST TITLE", fallback: "Memorize this host")
  /// Cannot open image file %@
  internal static func errorMessageCannotOpenImage(_ p1: Any) -> String {
    return L10n.tr("Localizable", "ERROR MESSAGE CANNOT OPEN IMAGE", String(describing: p1), fallback: "Cannot open image file %@")
  }
  /// Cannot open images
  internal static let errorMessageCannotOpenImages = L10n.tr("Localizable", "ERROR MESSAGE CANNOT OPEN IMAGES", fallback: "Cannot open images")
  /// Invalid image data for file %@
  internal static func errorMessageInvalidImageData(_ p1: Any) -> String {
    return L10n.tr("Localizable", "ERROR MESSAGE INVALID IMAGE DATA", String(describing: p1), fallback: "Invalid image data for file %@")
  }
  /// PDF file couldn't be generated
  internal static let errorMessagePdfCouldNotBeGenerated = L10n.tr("Localizable", "ERROR MESSAGE PDF COULD NOT BE GENERATED", fallback: "PDF file couldn't be generated")
  /// No images selected
  internal static let errorMessagePdfNoImages = L10n.tr("Localizable", "ERROR MESSAGE PDF NO IMAGES", fallback: "No images selected")
  /// Scan a document with one of the devices on the left and it will appear here
  internal static let galleryEmptySubtitle = L10n.tr("Localizable", "GALLERY EMPTY SUBTITLE", fallback: "Scan a document with one of the devices on the left and it will appear here")
  /// No images yet
  internal static let galleryEmptyTitle = L10n.tr("Localizable", "GALLERY EMPTY TITLE", fallback: "No images yet")
  /// Scan
  internal static let galleryItem = L10n.tr("Localizable", "GALLERY ITEM", fallback: "Scan")
  /// %d of %d
  internal static func galleryItemNOfN(_ p1: Int, _ p2: Int) -> String {
    return L10n.tr("Localizable", "GALLERY ITEM N OF N", p1, p2, fallback: "%d of %d")
  }
  /// All scans
  internal static let galleryOverviewTitle = L10n.tr("Localizable", "GALLERY OVERVIEW TITLE", fallback: "All scans")
  /// Loading...
  internal static let loading = L10n.tr("Localizable", "LOADING", fallback: "Loading...")
  /// Cancel
  internal static let mailAlertCancel = L10n.tr("Localizable", "MAIL ALERT CANCEL", fallback: "Cancel")
  /// Choose your preferred service
  internal static let mailAlertMessage = L10n.tr("Localizable", "MAIL ALERT MESSAGE", fallback: "Choose your preferred service")
  /// Send an email
  internal static let mailAlertTitle = L10n.tr("Localizable", "MAIL ALERT TITLE", fallback: "Send an email")
  /// Copy email to pasteboard
  internal static let mailCopyPasteboardName = L10n.tr("Localizable", "MAIL COPY PASTEBOARD NAME", fallback: "Copy email to pasteboard")
  /// Copied to your pasteboard
  internal static let mailCopyPasteboardSuccess = L10n.tr("Localizable", "MAIL COPY PASTEBOARD SUCCESS", fallback: "Copied to your pasteboard")
  /// Abort current operation
  internal static let menuAbort = L10n.tr("Localizable", "MENU ABORT", fallback: "Abort current operation")
  /// New host...
  internal static let menuAddHost = L10n.tr("Localizable", "MENU ADD HOST", fallback: "New host...")
  /// Open image gallery
  internal static let menuOpenGallery = L10n.tr("Localizable", "MENU OPEN GALLERY", fallback: "Open image gallery")
  /// Preferences...
  internal static let menuPreferences = L10n.tr("Localizable", "MENU PREFERENCES", fallback: "Preferences...")
  /// Preview
  internal static let menuPreview = L10n.tr("Localizable", "MENU PREVIEW", fallback: "Preview")
  /// Refresh
  internal static let menuRefresh = L10n.tr("Localizable", "MENU REFRESH", fallback: "Refresh")
  /// Scan
  internal static let menuScan = L10n.tr("Localizable", "MENU SCAN", fallback: "Scan")
  /// Off
  internal static let optionBoolOff = L10n.tr("Localizable", "OPTION BOOL OFF", fallback: "Off")
  /// On
  internal static let optionBoolOn = L10n.tr("Localizable", "OPTION BOOL ON", fallback: "On")
  /// Auto
  internal static let optionValueAuto = L10n.tr("Localizable", "OPTION VALUE AUTO", fallback: "Auto")
  /// Tap to delete all cached data
  internal static let preferencesMessageCleanupCache = L10n.tr("Localizable", "PREFERENCES MESSAGE CLEANUP CACHE", fallback: "Tap to delete all cached data")
  /// Each event will be sent along with your iOS version, device model and app version. Event-specific data are the number of hosts you have configured, the make of your scanner and possible errors you encountered.
  internal static let preferencesMessageEnableAnalytics = L10n.tr("Localizable", "PREFERENCES MESSAGE ENABLE ANALYTICS", fallback: "Each event will be sent along with your iOS version, device model and app version. Event-specific data are the number of hosts you have configured, the make of your scanner and possible errors you encountered.")
  /// By default scanned documents will be saved as JPEG images with a 90%% quality factor. Toggle this setting to save as a lossless PNG file or a 90%% quality HEIC file
  internal static let preferencesMessageImageFormat = L10n.tr("Localizable", "PREFERENCES MESSAGE IMAGE FORMAT", fallback: "By default scanned documents will be saved as JPEG images with a 90%% quality factor. Toggle this setting to save as a lossless PNG file or a 90%% quality HEIC file")
  /// When creating a PDF from scanned images, the pages size can be either the size of each individual scan, set to A4 page size, or US letter
  internal static let preferencesMessagePdfSize = L10n.tr("Localizable", "PREFERENCES MESSAGE PDF SIZE", fallback: "When creating a PDF from scanned images, the pages size can be either the size of each individual scan, set to A4 page size, or US letter")
  /// Will use the default color mode and image intensity, if those options are found and support using an automatic value. Allows for more consistent render accross previews, whatever the option you set, but can reduce preview speed and reset other options depending on the device. This can result in black images when scanning after a preview on some SANE backends (e.g. Pixma 1.0.24).
  internal static let preferencesMessagePreviewDefaultColorMode = L10n.tr("Localizable", "PREFERENCES MESSAGE PREVIEW DEFAULT COLOR MODE", fallback: "Will use the default color mode and image intensity, if those options are found and support using an automatic value. Allows for more consistent render accross previews, whatever the option you set, but can reduce preview speed and reset other options depending on the device. This can result in black images when scanning after a preview on some SANE backends (e.g. Pixma 1.0.24).")
  /// Some devices have advanced options, toggling this setting ON will allow you to access them.
  internal static let preferencesMessageShowAdvancedOptions = L10n.tr("Localizable", "PREFERENCES MESSAGE SHOW ADVANCED OPTIONS", fallback: "Some devices have advanced options, toggling this setting ON will allow you to access them.")
  /// About this app
  internal static let preferencesSectionAboutApp = L10n.tr("Localizable", "PREFERENCES SECTION ABOUT APP", fallback: "About this app")
  /// Analytics
  internal static let preferencesSectionAnalytics = L10n.tr("Localizable", "PREFERENCES SECTION ANALYTICS", fallback: "Analytics")
  /// Miscellaneous
  internal static let preferencesSectionMisc = L10n.tr("Localizable", "PREFERENCES SECTION MISC", fallback: "Miscellaneous")
  /// Preview
  internal static let preferencesSectionPreview = L10n.tr("Localizable", "PREFERENCES SECTION PREVIEW", fallback: "Preview")
  /// Scan
  internal static let preferencesSectionScan = L10n.tr("Localizable", "PREFERENCES SECTION SCAN", fallback: "Scan")
  /// Preferences
  internal static let preferencesTitle = L10n.tr("Localizable", "PREFERENCES TITLE", fallback: "Preferences")
  /// Acknowledgements
  internal static let preferencesTitleAcknowledgements = L10n.tr("Localizable", "PREFERENCES TITLE ACKNOWLEDGEMENTS", fallback: "Acknowledgements")
  /// App version
  internal static let preferencesTitleAppVersion = L10n.tr("Localizable", "PREFERENCES TITLE APP VERSION", fallback: "App version")
  /// Cached data (thumbnails, etc)
  internal static let preferencesTitleCleanupCache = L10n.tr("Localizable", "PREFERENCES TITLE CLEANUP CACHE", fallback: "Cached data (thumbnails, etc)")
  /// Got some questions? Drop me an email!
  internal static let preferencesTitleContact = L10n.tr("Localizable", "PREFERENCES TITLE CONTACT", fallback: "Got some questions? Drop me an email!")
  /// App analytics
  internal static let preferencesTitleEnableAnalytics = L10n.tr("Localizable", "PREFERENCES TITLE ENABLE ANALYTICS", fallback: "App analytics")
  /// Image format
  internal static let preferencesTitleImageFormat = L10n.tr("Localizable", "PREFERENCES TITLE IMAGE FORMAT", fallback: "Image format")
  /// PDF dimensions
  internal static let preferencesTitlePdfSize = L10n.tr("Localizable", "PREFERENCES TITLE PDF SIZE", fallback: "PDF dimensions")
  /// Preview with default color mode
  internal static let preferencesTitlePreviewDefaultColorMode = L10n.tr("Localizable", "PREFERENCES TITLE PREVIEW DEFAULT COLOR MODE", fallback: "Preview with default color mode")
  /// Sane backends version
  internal static let preferencesTitleSaneVersion = L10n.tr("Localizable", "PREFERENCES TITLE SANE VERSION", fallback: "Sane backends version")
  /// Show advanced options
  internal static let preferencesTitleShowAdvancedOptions = L10n.tr("Localizable", "PREFERENCES TITLE SHOW ADVANCED OPTIONS", fallback: "Show advanced options")
  /// A4
  internal static let preferencesValuePdfSizeA4 = L10n.tr("Localizable", "PREFERENCES VALUE PDF SIZE A4", fallback: "A4")
  /// Original size
  internal static let preferencesValuePdfSizeImageSize = L10n.tr("Localizable", "PREFERENCES VALUE PDF SIZE IMAGE SIZE", fallback: "Original size")
  /// US Letter
  internal static let preferencesValuePdfSizeUsLetter = L10n.tr("Localizable", "PREFERENCES VALUE PDF SIZE US LETTER", fallback: "US Letter")
  /// After selecting a device you'll have the ability to preview then scan a document
  internal static let previewVcNoDeviceSubtitle = L10n.tr("Localizable", "PREVIEW VC NO DEVICE SUBTITLE", fallback: "After selecting a device you'll have the ability to preview then scan a document")
  /// Select a device
  internal static let previewVcNoDeviceTitle = L10n.tr("Localizable", "PREVIEW VC NO DEVICE TITLE", fallback: "Select a device")
  /// Generating preview...
  internal static let previewing = L10n.tr("Localizable", "PREVIEWING", fallback: "Generating preview...")
  /// Generating preview... (%d%%)
  internal static func previewingProgress(_ p1: Int) -> String {
    return L10n.tr("Localizable", "PREVIEWING PROGRESS", p1, fallback: "Generating preview... (%d%%)")
  }
  /// Done scanning!
  internal static let scanFinishedNotificationTitle = L10n.tr("Localizable", "SCAN FINISHED NOTIFICATION TITLE", fallback: "Done scanning!")
  /// Scanning...
  internal static let scanning = L10n.tr("Localizable", "SCANNING", fallback: "Scanning...")
  /// Scanning... (%d%%)
  internal static func scanningProgress(_ p1: Int) -> String {
    return L10n.tr("Localizable", "SCANNING PROGRESS", p1, fallback: "Scanning... (%d%%)")
  }
  /// Scanning... (%d%%, finished %d documents)
  internal static func scanningProgressCount(_ p1: Int, _ p2: Int) -> String {
    return L10n.tr("Localizable", "SCANNING PROGRESS COUNT", p1, p2, fallback: "Scanning... (%d%%, finished %d documents)")
  }
  /// Share as PDF
  internal static let shareAsPdf = L10n.tr("Localizable", "SHARE AS PDF", fallback: "Share as PDF")
  /// Share as PDF (interleave front and back sides)
  internal static let shareAsPdfInterleaved = L10n.tr("Localizable", "SHARE AS PDF INTERLEAVED", fallback: "Share as PDF (interleave front and back sides)")
  /// Warming up
  internal static let warmingUp = L10n.tr("Localizable", "WARMING UP", fallback: "Warming up")
  internal enum DialogDeleteScansMessage {
    /// Are you sure you want to delete this scan?
    internal static let one = L10n.tr("Localizable", "DIALOG DELETE SCANS MESSAGE.one", fallback: "Are you sure you want to delete this scan?")
    /// Are you sure you want to delete %d scans?
    internal static func other(_ p1: Int) -> String {
      return L10n.tr("Localizable", "DIALOG DELETE SCANS MESSAGE.other", p1, fallback: "Are you sure you want to delete %d scans?")
    }
    /// Are you sure you want to delete these scans?
    internal static let zero = L10n.tr("Localizable", "DIALOG DELETE SCANS MESSAGE.zero", fallback: "Are you sure you want to delete these scans?")
  }
  internal enum GalleryItemsCount {
    /// 1 scan
    internal static let one = L10n.tr("Localizable", "GALLERY ITEMS COUNT.one", fallback: "1 scan")
    /// %d scans
    internal static func other(_ p1: Int) -> String {
      return L10n.tr("Localizable", "GALLERY ITEMS COUNT.other", p1, fallback: "%d scans")
    }
    /// No scans
    internal static let zero = L10n.tr("Localizable", "GALLERY ITEMS COUNT.zero", fallback: "No scans")
  }
  internal enum GallerySelectedItemsCount {
    /// One scan selected
    internal static let one = L10n.tr("Localizable", "GALLERY SELECTED ITEMS COUNT.one", fallback: "One scan selected")
    /// %d scans selected
    internal static func other(_ p1: Int) -> String {
      return L10n.tr("Localizable", "GALLERY SELECTED ITEMS COUNT.other", p1, fallback: "%d scans selected")
    }
    /// No scan selected
    internal static let zero = L10n.tr("Localizable", "GALLERY SELECTED ITEMS COUNT.zero", fallback: "No scan selected")
  }
  internal enum ScanFinishedNotificationMessage {
    /// 1 document saved
    internal static let one = L10n.tr("Localizable", "SCAN FINISHED NOTIFICATION MESSAGE.one", fallback: "1 document saved")
    /// %d documents saved
    internal static func other(_ p1: Int) -> String {
      return L10n.tr("Localizable", "SCAN FINISHED NOTIFICATION MESSAGE.other", p1, fallback: "%d documents saved")
    }
    /// 0 documents saved
    internal static let zero = L10n.tr("Localizable", "SCAN FINISHED NOTIFICATION MESSAGE.zero", fallback: "0 documents saved")
  }
}
// swiftlint:enable explicit_type_interface function_parameter_count identifier_name line_length
// swiftlint:enable nesting type_body_length type_name vertical_whitespace_opening_braces

// MARK: - Implementation Details

extension L10n {
  private static func tr(_ table: String, _ key: String, _ args: CVarArg..., fallback value: String) -> String {
    let format = BundleToken.bundle.localizedString(forKey: key, value: value, table: table)
    return String(format: format, locale: Locale.current, arguments: args)
  }
}

// swiftlint:disable convenience_type
private final class BundleToken {
  static let bundle: Bundle = {
    #if SWIFT_PACKAGE
    return Bundle.module
    #else
    return Bundle(for: BundleToken.self)
    #endif
  }()
}
// swiftlint:enable convenience_type
