//
//  SYSaneHelper.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneHelper.h"
#import "SYTools.h"
#import "sane.h"

@implementation SYSaneHelper

+ (void)updateSaneNetConfig
{
    NSArray *hosts = [NSArray arrayWithContentsOfFile:[SYTools hostsFile]];
    
    NSString *hostsString = [hosts componentsJoinedByString:@"\n"];
    NSString *config = [NSString stringWithFormat:@"connect_timeout = 30\n%@", (hostsString ?: @"")];
    
    NSString *configPath = [[SYTools documentsPath] stringByAppendingPathComponent:@"net.conf"];
    [config writeToFile:configPath atomically:YES encoding:NSUTF8StringEncoding error:NULL];
}

+ (void)listDevices:(void(^)(NSArray *devices, NSString *error))completion
{
    void(^compl)(NSArray *devices, SANE_Status s) = ^(NSArray *devices, SANE_Status s)
    {
        if(!completion)
            return;
        
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(devices, (s == SANE_STATUS_GOOD ? nil : status));
        });
    };
    
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        SANE_Status s = sane_init(NULL, NULL);
        if(s != SANE_STATUS_GOOD)
        {
            compl(nil, s);
            return;
        }
        
        const SANE_Device **devices = NULL;
        s = sane_get_devices(&devices, SANE_FALSE);
        if(s != SANE_STATUS_GOOD)
        {
            compl(nil, s);
            return;
        }
        
        NSMutableArray *devicesNames = [NSMutableArray array];
        
        uint i = 0;
        while(devices[i])
        {
            const SANE_Device *d = devices[i];
            NSString *deviceName = [NSString stringWithFormat:@"%s %s %s (%s)", d->name, d->vendor, d->model, d->type];
            [devicesNames addObject:deviceName];
            ++i;
        }
        
        sane_exit();
        compl(devicesNames, SANE_STATUS_GOOD);
    });
}

@end
