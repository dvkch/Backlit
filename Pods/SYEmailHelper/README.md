SYEmailHelper
=============

Easily detect installed email clients and allow the user to choose one when sending an email.

I use it to quickly add a "Contact me" feature in my apps. This will detect a list of `SYEmailService`, representing:

- native in-app composer
- third party apps
- last resort: copy email address to clipboard, to let your user send via an unsupported app

If there are no items in the list the completion block is called with an error: `SYEmailHelperErrorDomain`, code `SYEmailHelperErrorCode_NoServiceAvailable`. 

If the use selected a third party app that couldn't be opened an error will be generated too: `SYEmailHelperErrorCode_CouldntOpenApp`.

If a single supported service is available it will automatically be launched.

If multiple supported services are available an action sheet will be displayed.

You can:

- disable the "copy to pasteboard" option
- change the text for "copy to pasteboard" option
- change the text for "cancel" option

Example:

    [SYEmailServicePasteboard setName:@"Copy email address to pasteboard"];
    [[SYEmailHelper shared] setShowCopyToPasteboard:YES];
    [[SYEmailHelper shared] setActionSheetTitleText:@"Which app you wanna use bro?"];
    [[SYEmailHelper shared] setActionSheetCancelButtonText:@"fuhgeddaboudit"];
    [[SYEmailHelper shared] composeEmailWithAddress:self.fieldEmail.text
                                            subject:self.fieldSubject.text
                                               body:self.fieldBody.text
                                       presentingVC:self
                                         completion:
     ^(BOOL userCancelled, SYEmailService *service, NSError *error)
    {
        if (userCancelled)
            self.labelCompletion.text = @"User cancelled";
        else if (service && error)
            self.labelCompletion.text = [NSString stringWithFormat:@"Service %@ encountered error: %@", service.name, error.localizedDescription];
        else if (error)
            self.labelCompletion.text = [NSString stringWithFormat:@"Encountered error: %@", error.localizedDescription];
        else
            self.labelCompletion.text = [NSString stringWithFormat:@"No error, used service %@", service.name];
    }];

Don't forget
============

You'll need to add the list of supported apps inside your `Info.plist`. Here are the needed items for the current list of supported apps.

	<key>LSApplicationQueriesSchemes</key>
	<array>
		<string>googlegmail</string>
		<string>inbox-gmail</string>
		<string>ms-outlook</string>
		<string>readdle-spark</string>
	</array>

Screenshots
===========

![ActionSheet](https://raw.githubusercontent.com/dvkch/SYEmailHelper/master/screenshots/screenshot_choices.PNG)


License
===

Use it as you like in every project you want, redistribute with mentions of my name and don't blame me if it breaks :)

-- dvkch
 
