//
//  SYPDFActivity.m
//  SaneScanner
//
//  Created by Stanislas Chevallier on 19/04/2017.
//  Copyright © 2017 Syan. All rights reserved.
//

#import "SYPDFActivity.h"
#import <NSData+SYKit.h>
#import "SYPDFMaker.h"
#import "SVProgressHUD+SY.h"
#import "SYGalleryManager.h"

@interface SYPDFActivity ()
@property (nonatomic, strong) NSArray <NSURL *> *items;

@property (nonatomic, weak)   UIView                    *sourceView;
@property (nonatomic, weak)   UIBarButtonItem           *barButtonItem;
@property (nonatomic, assign) CGRect                    sourceRect;
@property (nonatomic, assign) UIPopoverArrowDirection   permittedArrowDirections;

@end

@implementation SYPDFActivity

- (void)configureWithOriginalActivityViewController:(UIActivityViewController *)originalActivityVC
{
    UIPopoverPresentationController *pc = originalActivityVC.popoverPresentationController;
    self.barButtonItem              = pc.barButtonItem;
    self.sourceView                 = pc.sourceView;
    self.sourceRect                 = pc.sourceRect;
    self.permittedArrowDirections   = pc.permittedArrowDirections;
}

- (UIActivityType)activityType
{
    return [[[NSBundle mainBundle] bundleIdentifier] stringByAppendingString:$$(".pdfActivity")];
}

- (NSString *)activityTitle
{
    return $("SHARE AS PDF");
}

- (UIImage *)activityImage
{
    return [UIImage imageNamed:$$("make_pdf_activity")];
}

+ (UIActivityCategory)activityCategory
{
    return UIActivityCategoryAction;
}

- (BOOL)canPerformWithActivityItems:(NSArray *)activityItems
{
    for (id item in activityItems)
    {
        if (![item isKindOfClass:[NSURL class]])
            return NO;
        
        NSURL *url = item;
        NSData *data = [[NSData alloc] initWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:NULL];
        BOOL isImage = ([data sy_imageDataIsValidPNG] || [data sy_imageDataIsValidJPEG]);
        if (!isImage)
            return NO;
    }
    
    return YES;
}

- (void)prepareWithActivityItems:(NSArray *)activityItems
{
    NSSortDescriptor *sortDescriptor = [NSSortDescriptor sortDescriptorWithKey:$$("lastPathComponent") ascending:YES];
    self.items = [activityItems sortedArrayUsingDescriptors:@[sortDescriptor]];
    [SVProgressHUD show];
}

- (void)performActivity
{
    NSString *tempPath = [[SYGalleryManager shared] tempPDFPath];
    
    BOOL result = [SYPDFMaker createPDFAtURL:[NSURL fileURLWithPath:tempPath]
                            fromImagesAtURLs:self.items
                                 aspectRatio:210./297.
                                 jpegQuality:JPEG_COMP
                               fixedPageSize:YES];
    
    if (!result || ![[NSFileManager defaultManager] fileExistsAtPath:tempPath])
    {
        [SVProgressHUD showErrorWithStatus:$("ERROR MESSAGE PDF NOT CREATED")];
        [self activityDidFinish:NO];
        return;
    }
    
    [SVProgressHUD dismiss];
    
    UIActivityViewController *activityViewController =
    [[UIActivityViewController alloc] initWithActivityItems:@[[NSURL fileURLWithPath:tempPath]]
                                      applicationActivities:nil];
    
    UIPopoverPresentationController *pc = activityViewController.popoverPresentationController;
    pc.barButtonItem            = self.barButtonItem;
    pc.sourceView               = self.sourceView;
    pc.sourceRect               = self.sourceRect;
    pc.permittedArrowDirections = self.permittedArrowDirections;

    [activityViewController setCompletionWithItemsHandler:^(UIActivityType activityType, BOOL completed, NSArray *returnedItems, NSError *activityError)
     {
         // is called when the interaction with the PDF is done. It's either been copied, imported,
         // displayed, shared or printed, but we can dispose of it
         [[SYGalleryManager shared] deleteTempPDFs];
         [self activityDidFinish:completed];
     }];
    
    [self.presentingViewController presentViewController:activityViewController animated:YES completion:nil];
}

@end
