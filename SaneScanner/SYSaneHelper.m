//
//  SYSaneHelper.m
//  SaneScanner
//
//  Created by rominet on 06/05/15.
//  Copyright (c) 2015 Syan. All rights reserved.
//

#import "SYSaneHelper.h"
#import "SYTools.h"
#import "SYSaneDevice.h"
#import "SYSaneOption.h"
#import "SYSaneOptionBool.h"
#import "SYSaneOptionNumber.h"
#import "SYSaneOptionString.h"
#import "SYSaneOptionButton.h"
#import "SYSaneOptionGroup.h"
#import "SYSaneScanParameters.h"
#import "sane.h"
#import "UIImage+SY.h"
#import "NSMutableData+SY.h"
#import "NSObject+SY.h"

#define ENSURE_RUNNING_ON_SANE_THREAD(block) \
if ([NSThread currentThread] != self.thread) { [self performBlock:block onThread:self.thread]; return; }

#define PERF_START()    \
NSDate *perfDate = [NSDate date]

#define PERF_END(min)   \
if ([[NSDate date] timeIntervalSinceDate:perfDate] > ((double)min)/1000.) \
    NSLog(@"%@: %.03lfs", NSStringFromSelector(_cmd), [[NSDate date] timeIntervalSinceDate:perfDate])

#define PERF_MIN_MS (10000)

void sane_auth(SANE_String_Const resource, SANE_Char *username, SANE_Char *password);

@interface SYSaneHelper ()
@property (atomic, strong) NSThread *thread;
@property (atomic, strong) NSMutableArray <NSString *> *hosts;
@property (atomic, strong) NSArray <SYSaneDevice *> *devices;
@property (atomic, strong) NSDictionary <NSString *, NSValue *> *openedDevices;
@end

@implementation SYSaneHelper

+ (instancetype)shared
{
    static dispatch_once_t once;
    static id sharedInstance;
    dispatch_once(&once, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        self.thread = [[NSThread alloc] initWithTarget:self selector:@selector(threadKeepAlive) object:nil];
        [self.thread setName:@"SYSaneHelper"];
        [self.thread start];
        
        NSArray <NSString *> *savedHosts = [NSArray arrayWithContentsOfFile:[SYTools hostsFile]];
        self.hosts = [NSMutableArray arrayWithArray:savedHosts];
        
        [self performBlock:^{
            [self initSane];
        } onThread:self.thread];
    }
    return self;
}

- (void)dealloc
{
    sane_exit();
}

- (void)initSane
{
    SANE_Status s = sane_init(NULL, NULL);
    if(s != SANE_STATUS_GOOD)
        self.saneInitError = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
}

- (void)threadKeepAlive
{
    NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
    BOOL running = true;
    [[NSRunLoop currentRunLoop] addPort:[NSMachPort port] forMode:NSDefaultRunLoopMode];
    while (running && [runLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]]) {
        //run loop spinned ones
    }
}

#pragma mark - Hosts

- (NSArray <NSString *> *)allHosts
{
    return [self.hosts copy];
}

- (void)addHost:(NSString *)host
{
    if (!host.length)
        return;
    
    [self.hosts addObject:host];
    [self commitHosts];
    [self updateDevices];
}

- (void)removeHost:(NSString *)host
{
    if (!host.length)
        return;
    
    [self.hosts removeObject:host];
    [self commitHosts];
    [self updateDevices];
}

- (void)commitHosts
{
    [[self.hosts copy] writeToFile:[SYTools hostsFile] atomically:YES];
    
    NSString *hostsString = [self.hosts componentsJoinedByString:@"\n"];
    NSString *config = [NSString stringWithFormat:@"connect_timeout = 30\n%@", (hostsString ?: @"")];
    
    NSString *configPath = [[SYTools documentsPath] stringByAppendingPathComponent:@"net.conf"];
    [config writeToFile:configPath atomically:YES encoding:NSUTF8StringEncoding error:NULL];
}

#pragma mark - Devices

- (NSArray <SYSaneDevice *> *)allDevices
{
    return [self.devices copy];
}

- (void)setIsUpdatingDevices:(BOOL)isUpdatingDevices
{
    @synchronized(self.thread) {
        self->_isUpdatingDevices = isUpdatingDevices;
        dispatch_async(dispatch_get_main_queue(), ^{
            if(isUpdatingDevices)
                [self.delegate saneHelperDidStartUpdatingDevices:self];
            if(!isUpdatingDevices)
                [self.delegate saneHelperDidEndUpdatingDevices:self];
        });
    }
}

- (void)updateDevices
{
    if(self.isUpdatingDevices)
        return;
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self updateDevices];
    });
    
    PERF_START();
    
    self.isUpdatingDevices = YES;
    
    // refreshing without restarting sane first causes lots of crashs
    /*
    {
        sane_exit();
        SANE_Status s = sane_init(NULL, sane_auth);
        if(s != SANE_STATUS_GOOD)
        {
            NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
            self.devices = nil;
            self.saneLastGetDevicesError = status;
            self.isUpdatingDevices = NO;
            return;
        }
    }
    */
    
    const SANE_Device **devices = NULL;
    SANE_Status s = sane_get_devices(&devices, SANE_FALSE);
    if(s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        self.devices = nil;
        self.saneLastGetDevicesError = status;
        self.isUpdatingDevices = NO;
        return;
    }
    
    NSMutableArray *devicesObjects = [NSMutableArray array];
    
    uint i = 0;
    while(devices[i])
    {
        [devicesObjects addObject:[[SYSaneDevice alloc] initWithCDevice:devices[i]]];
        ++i;
    }
    
    self.devices = [devicesObjects copy];
    self.saneLastGetDevicesError = nil;
    self.isUpdatingDevices = NO;
    
    PERF_END(PERF_MIN_MS);
}

- (void)openDevice:(SYSaneDevice *)device block:(void(^)(NSString *error))block
{
    if(!device)
        return;
    
    if([[self.openedDevices allKeys] containsObject:device.name])
        return;
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self openDevice:device block:block];
    });
    
    SANE_Handle h = NULL;
    
    PERF_START();
    SANE_Status s = sane_open([device.name cStringUsingEncoding:NSUTF8StringEncoding], &h);
    PERF_END(PERF_MIN_MS);
    
    if(s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if(block)
                block(status);
        });
        return;
    }
    
    NSMutableDictionary *openedDevices = [NSMutableDictionary dictionaryWithDictionary:self.openedDevices];
    [openedDevices setObject:[NSValue valueWithPointer:h] forKey:device.name];
    self.openedDevices = [openedDevices copy];

    dispatch_async(dispatch_get_main_queue(), ^{
        if(block)
            block(nil);
    });
}

- (void)closeDevice:(SYSaneDevice *)device
{
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self closeDevice:device];
    });
    
    if(![[self.openedDevices allKeys] containsObject:device.name])
        return;
    
    NSValue *value = self.openedDevices[device.name];
    SANE_Handle h = [value pointerValue];
    PERF_START();
    sane_close(h);
    PERF_END(PERF_MIN_MS);

    NSMutableDictionary *openedDevices = [NSMutableDictionary dictionaryWithDictionary:self.openedDevices];
    [openedDevices removeObjectForKey:device.name];
    self.openedDevices = [openedDevices copy];
}

- (BOOL)isDeviceOpen:(SYSaneDevice *)device
{
    if(!device)
        return NO;
    
    return [[self.openedDevices allKeys] containsObject:device.name];
}

- (void)askAuthForDevice:(NSString *)device outUsername:(NSString **)username outPassword:(NSString **)password
{
    [self.delegate saneHelper:self needsAuthForDevice:device outUsername:username outPassword:password];
}

#pragma mark - Options

- (void)listOptionsForDevice:(SYSaneDevice *)device block:(void(^)(void))block
{
    if(![self isDeviceOpen:device])
        return;
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self listOptionsForDevice:device block:block];
    });
    
    PERF_START();
    
    NSValue *value = self.openedDevices[device.name];
    SANE_Handle h = [value pointerValue];
    
    SANE_Int count;
    
    SANE_Status s = sane_control_option(h, 0, SANE_ACTION_GET_VALUE, &count, NULL);
    
    NSMutableArray *opts = [NSMutableArray array];
    for(uint i = 1; i < count; ++i)
    {
        const SANE_Option_Descriptor *descriptor = sane_get_option_descriptor(h, i);
        [opts addObject:[SYSaneOption initWithCOpt:descriptor index:i device:device]];
    }
    
    for(SYSaneOption *d in opts)
        [d refreshValue:nil];
    
    PERF_END(PERF_MIN_MS);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [device setOptions:[opts copy]];
        if(block)
            block();
    });
}

- (void)getValueForOption:(SYSaneOption *)option
                    block:(void(^)(id value, NSString *error))block
{
    if (![self isDeviceOpen:option.device]) {
        if (block)
            block(nil, @"Device not opened");
        return;
    }
    
    if (option.type == SANE_TYPE_GROUP) {
        if (block)
            block(nil, @"No value for type GROUP");
        return;
    }
    
    if (option.type == SANE_TYPE_BUTTON) {
        if (block)
            block(nil, @"No value for type BUTTON");
        return;
    }
    
    if (option.capInactive) {
        if (block)
            block(nil, @"Cannot get value for inactive option");
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self getValueForOption:option block:block];
    });

    NSValue *handle = self.openedDevices[option.device.name];
    SANE_Handle h = [handle pointerValue];
    
    void *value = malloc(option.size);
    PERF_START();
    SANE_Status s = sane_control_option(h, option.index, SANE_ACTION_GET_VALUE, value, NULL);
    PERF_END(PERF_MIN_MS);

    if(s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (block)
                block(nil, status);
        });
        return;
    }
    
    id castedValue;
    if(option.type == SANE_TYPE_BOOL) {
        SANE_Bool *bValue = value;
        castedValue = [NSNumber numberWithBool:bValue[0]];
    }
    if(option.type == SANE_TYPE_INT || option.type == SANE_TYPE_FIXED) {
        SANE_Int *iValue = value;
        castedValue = [NSNumber numberWithInt:iValue[0]];
    }
    if(option.type == SANE_TYPE_STRING) {
        SANE_String sValue = value;
        castedValue = [NSString stringWithCString:sValue encoding:NSUTF8StringEncoding];
    }
    
    free(value);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        if (block)
            block(castedValue, nil);
    });
}

- (void)setCropArea:(CGRect)cropArea
            useAuto:(BOOL)useAuto
          forDevice:(SYSaneDevice *)device
              block:(void(^)(BOOL reloadAllOptions, NSString *error))block
{
    if(![self isDeviceOpen:device]) {
        if (block)
            block(NO, @"Device not opened");
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self setCropArea:cropArea useAuto:useAuto forDevice:device block:block];
    });
    
    __block BOOL finalReloadAllOptions;
    __block NSString *finalError;
    
    NSArray <NSNumber *> *stdOptions = @[@(SYSaneStandardOptionAreaTopLeftX),
                                         @(SYSaneStandardOptionAreaTopLeftY),
                                         @(SYSaneStandardOptionAreaBottomRightX),
                                         @(SYSaneStandardOptionAreaBottomRightY)];
    
    NSArray <NSNumber *> *values = @[@(cropArea.origin.x),
                                     @(cropArea.origin.y),
                                     @(CGRectGetMaxX(cropArea)),
                                     @(CGRectGetMaxY(cropArea))];
    
    for (NSUInteger i = 0; i < stdOptions.count; ++i)
    {
        SYSaneStandardOption stdOpt = stdOptions[i].unsignedIntegerValue;
        SYSaneOptionNumber *option = (SYSaneOptionNumber *)[device standardOption:stdOpt];
        [self setValue:values[i] orAutoValue:NO forOption:option block:^(BOOL reloadAllOptions, NSString *error) {
            finalReloadAllOptions = finalReloadAllOptions || reloadAllOptions;
            finalError = error;
        }];
        
        if (finalError)
        {
            NSLog(@"%@: set %@ to %@\n%@", finalError, option.name, values[i], option.descriptionHuman);
            dispatch_async(dispatch_get_main_queue(), ^{
                if (block) block(finalReloadAllOptions, finalError);
            });
            return;
        }
    }
    
    dispatch_async(dispatch_get_main_queue(), ^{
        if (block) block(finalReloadAllOptions, finalError);
    });
}

- (void)setValue:(id)value
     orAutoValue:(BOOL)autoValue
       forOption:(SYSaneOption *)option
           block:(void(^)(BOOL reloadAllOptions, NSString *error))block
{
    if(![self isDeviceOpen:option.device]) {
        if (block)
            block(NO, @"Device not opened");
        return;
    }
    
    if(option.type == SANE_TYPE_GROUP) {
        if (block)
            block(NO, @"No value for type GROUP");
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self setValue:value orAutoValue:autoValue forOption:option block:block];
    });
    
    NSValue *handle = self.openedDevices[option.device.name];
    SANE_Handle h = [handle pointerValue];
    
    void *byteValue = NULL;
    
    if (!autoValue) {
        if(option.type == SANE_TYPE_BOOL) {
            byteValue = malloc(option.size);
            ((SANE_Bool *)byteValue)[0] = [value boolValue];
        }
        else if(option.type == SANE_TYPE_INT) {
            byteValue = malloc(option.size);
            ((SANE_Int *)byteValue)[0] = [value intValue];
        }
        else if(option.type == SANE_TYPE_FIXED) {
            byteValue = malloc(option.size);
            ((SANE_Fixed *)byteValue)[0] = SANE_FIX([value doubleValue]);
        }
        else if(option.type == SANE_TYPE_STRING) {
            byteValue = (SANE_String)[value cStringUsingEncoding:NSUTF8StringEncoding];
        }
    }
    
    SANE_Int info;
    SANE_Status s;
    
    PERF_START();
    if (autoValue)
        s = sane_control_option(h, option.index, SANE_ACTION_SET_AUTO, NULL, &info);
    else
        s = sane_control_option(h, option.index, SANE_ACTION_SET_VALUE, byteValue, &info);
    PERF_END(PERF_MIN_MS);
    
    if (s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        NSLog(@"--> %@ %lf", status, SANE_UNFIX(3061690));
    }
    
    BOOL reloadAllOptions = (info & SANE_INFO_RELOAD_OPTIONS || info & SANE_INFO_RELOAD_PARAMS);
    BOOL updatedValue = NO;
    
    if (s == SANE_STATUS_GOOD && !autoValue)
    {
        if(option.type == SANE_TYPE_BOOL) {
            SYSaneOptionBool *castedOption = (SYSaneOptionBool *)option;
            [castedOption setValue:((SANE_Bool *)byteValue)[0]];
            updatedValue = YES;
        }
        else if(option.type == SANE_TYPE_INT) {
            SYSaneOptionNumber *castedOption = (SYSaneOptionNumber *)option;
            [castedOption setValue:@(((SANE_Int *)byteValue)[0])];
            updatedValue = YES;
        }
        else if(option.type == SANE_TYPE_FIXED) {
            SYSaneOptionNumber *castedOption = (SYSaneOptionNumber *)option;
            [castedOption setValue:@(SANE_UNFIX(((SANE_Fixed *)byteValue)[0]))];
            updatedValue = YES;
        }
        else if(option.type == SANE_TYPE_STRING) {
            SYSaneOptionString *castedOption = (SYSaneOptionString *)option;
            [castedOption setValue:[NSString stringWithCString:(SANE_String)byteValue encoding:NSUTF8StringEncoding]];
            updatedValue = YES;
        }
    }
    
    if (!autoValue && (option.type == SANE_TYPE_BOOL || option.type == SANE_TYPE_INT || option.type == SANE_TYPE_FIXED))
        free(byteValue);
    
    if (s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (block)
                block(NO, status);
        });
        return;
    }
    
    if (!reloadAllOptions &&!updatedValue)
        [option refreshValue:nil];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        if (block)
            block(reloadAllOptions, nil);
    });
}

- (void)previewWithDevice:(SYSaneDevice *)device
            progressBlock:(void (^)(float))progressBlock
             successBlock:(void (^)(UIImage *, NSString *))successBlock
{
    if(![self isDeviceOpen:device]) {
        if (successBlock)
            successBlock(nil, @"Device not opened");
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self previewWithDevice:device progressBlock:progressBlock successBlock:successBlock];
    });
    
    NSMutableDictionary <NSString *, NSNumber *> *oldOptions = [NSMutableDictionary dictionary];
    SYSaneOption *optionPreview = [device standardOption:SYSaneStandardOptionPreview];
    
    if (optionPreview)
    {
        [self setValue:@(YES) orAutoValue:NO forOption:optionPreview block:nil];
    }
    else
    {
        NSArray <SYSaneOption *> *options = [device standardOptions:@[@(SYSaneStandardOptionColorMode),
                                                                      @(SYSaneStandardOptionResolution),
                                                                      @(SYSaneStandardOptionAreaTopLeftX),
                                                                      @(SYSaneStandardOptionAreaTopLeftY),
                                                                      @(SYSaneStandardOptionAreaBottomRightX),
                                                                      @(SYSaneStandardOptionAreaBottomRightY)]];
        
        for (SYSaneOption *option in options)
        {
            SYSaneOptionNumber *castedOption = (SYSaneOptionNumber *)option;
            [oldOptions setObject:castedOption.value forKey:castedOption.name];
            
            SYSaneStandardOption stdOption = SYSaneStandardOptionFromNSString(castedOption.name);
            SYOptionValue bestValue = SYBestValueForPreviewValueForOption(stdOption);
            
            [self setValue:castedOption.bestValueForPreview
               orAutoValue:(bestValue == SYOptionValueAuto)
                 forOption:castedOption
                     block:nil];
        }
    }
    
    __block UIImage *previewImage;
    __block NSString *previewError;
    
    [self scanWithDevice:device
         useScanCropArea:NO
           progressBlock:progressBlock
            successBlock:^(UIImage *image, NSString *error)
    {
        previewImage = image;
        previewError = error;
    }];
    
    if (optionPreview)
    {
        [self setValue:@(NO) orAutoValue:NO forOption:optionPreview block:nil];
    }
    else
    {
        for (NSString *optionName in oldOptions.allKeys)
        {
            SYSaneOptionNumber *option = (SYSaneOptionNumber *)[device optionWithName:optionName];
            [self setValue:oldOptions[optionName] orAutoValue:NO forOption:option block:nil];
        }
    }
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [device setLastPreviewImage:previewImage];
        if (successBlock)
            successBlock(previewImage, previewError);
    });
}

- (void)scanWithDevice:(SYSaneDevice *)device
         progressBlock:(void (^)(float))progressBlock
          successBlock:(void (^)(UIImage *, NSString *))successBlock
{
    [self scanWithDevice:device
         useScanCropArea:YES
           progressBlock:progressBlock
            successBlock:successBlock];
}

- (void)scanWithDevice:(SYSaneDevice *)device
       useScanCropArea:(BOOL)useScanCropArea
         progressBlock:(void (^)(float))progressBlock
          successBlock:(void (^)(UIImage *, NSString *))successBlock
{
    if(![self isDeviceOpen:device]) {
        if (successBlock)
            successBlock(nil, @"Device not opened");
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self scanWithDevice:device progressBlock:progressBlock successBlock:successBlock];
    });
    
    NSValue *handle = self.openedDevices[device.name];
    SANE_Handle h = [handle pointerValue];
    
    if (device.canCrop && useScanCropArea)
        [self setCropArea:device.cropArea useAuto:NO forDevice:device block:nil];
    
    PERF_START();
    SANE_Status s = sane_start(h);
    PERF_END(PERF_MIN_MS);
    
    if(s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (successBlock)
                successBlock(nil, status);
        });
        return;
    }
    
    s = sane_set_io_mode(h, NO);
    if(s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (successBlock)
                successBlock(nil, status);
        });
        return;
    }
    
    NSMutableData *data = [NSMutableData data];
    SANE_Int bufferMaxSize = 100*1000;
    SANE_Byte *buffer = malloc(bufferMaxSize);
    SANE_Int bufferActualSize = 0;
    SYSaneScanParameters *parameters = 0;
    while (s == SANE_STATUS_GOOD)
    {
        if (progressBlock && parameters.fileSize) {
            dispatch_async(dispatch_get_main_queue(), ^{
                progressBlock((double)data.length / (double)parameters.fileSize);
            });
        }
        
        [data appendData:[NSData dataWithBytes:buffer length:bufferActualSize]];
        s = sane_read(h, buffer, bufferMaxSize, &bufferActualSize);
        
        if (!parameters)
        {
            SANE_Parameters params;
            sane_get_parameters(h, &params);
            parameters = [[SYSaneScanParameters alloc] initWithCParams:params];
            NSLog(@"%@", parameters);
        }
    }
    
    free(buffer);

    if(s != SANE_STATUS_EOF)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (successBlock)
                successBlock(nil, status);
        });
        return;
    }

    if (parameters.currentlyAcquiredChannel == SANE_FRAME_GRAY && parameters.depth == 1)
        [data invertBitByBit];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *error;
        UIImage *image = [UIImage imageFromRGBData:data saneParameters:parameters error:&error];
        
        if (successBlock)
            successBlock(image, error);
    });
}

@end

void sane_auth(SANE_String_Const resource, SANE_Char *username, SANE_Char *password)
{
    NSString *outUsername;
    NSString *outPassword;
    
    [[SYSaneHelper shared] askAuthForDevice:[NSString stringWithCString:resource encoding:NSUTF8StringEncoding]
                                outUsername:&outUsername
                                outPassword:&outPassword];
    
    if (outUsername)
        *username = (SANE_Char)[outUsername cStringUsingEncoding:NSUTF8StringEncoding];
    
    if (outPassword)
        *password = (SANE_Char)[outPassword cStringUsingEncoding:NSUTF8StringEncoding];
}
