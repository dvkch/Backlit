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
#import "SYSaneOptionInt.h"
#import "SYSaneOptionDouble.h"
#import "SYSaneOptionString.h"
#import "SYSaneOptionButton.h"
#import "SYSaneOptionGroup.h"
#import "SYSaneScanParameters.h"
#import "sane.h"
#import "NSArray+SY.h"
#import "UIImage+SY.h"

#define ON_MAIN_THREAD (0)

#if ON_MAIN_THREAD

#define ENSURE_RUNNING_ON_SANE_THREAD(sel, obj)

#else 

#define ENSURE_RUNNING_ON_SANE_THREAD(sel, obj) \
if([NSThread currentThread] != self.thread) \
{ \
[self performSelector:(sel ?: _cmd) onThread:self.thread withObject:obj waitUntilDone:NO];\
return; \
}

#endif

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
#if ON_MAIN_THREAD
        self.thread = [NSThread mainThread];
#else
        self.thread = [[NSThread alloc] initWithTarget:self selector:@selector(threadKeepAlive) object:nil];
        [self.thread setName:@"SYSaneHelper"];
        [self.thread start];
#endif
        
        NSArray <NSString *> *savedHosts = [NSArray arrayWithContentsOfFile:[SYTools hostsFile]];
        self.hosts = [NSMutableArray arrayWithArray:savedHosts];
        
#if !ON_MAIN_THREAD
        [self performSelector:@selector(initSane) onThread:self.thread withObject:nil waitUntilDone:YES];
#endif
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

- (NSArray<NSString *> *)hostsCopy
{
    return [self.hosts copy];
}

- (NSUInteger)hostsCount
{
    return [self.hosts count];
}

- (NSString *)hostAtIndex:(NSUInteger)index
{
    if(index >= [self hostsCount])
        return nil;
    return [self.hosts objectAtIndex:index];
}

- (NSArray *)addHost:(NSString *)host
{
    if([host length])
    {
        [self.hosts addObject:host];
        [self commitHosts];
        [self updateDevices];
    }
    return self.hosts;
}

- (NSArray *)removeHostAtIndex:(NSUInteger)index
{
    if(index < [self.hosts count])
    {
        [self.hosts removeObjectAtIndex:index];
        [self commitHosts];
        [self updateDevices];
    }
    return self.hosts;
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

- (NSArray<SYSaneDevice *> *)devicesCopy
{
    return [self.devices copy];
}

- (NSUInteger)devicesCount
{
    return [self.devices count];
}

- (SYSaneDevice *)deviceAtIndex:(NSUInteger)index
{
    if(index >= [self devicesCount])
        return nil;
    return [self.devices objectAtIndex:index];
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
    
    ENSURE_RUNNING_ON_SANE_THREAD(nil, nil);
    
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
    
    NSArray *args = block ? @[device, block] : @[device];
    ENSURE_RUNNING_ON_SANE_THREAD(@selector(openDevice:), args);
    
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

// for performSelector: invocation
- (void)openDevice:(NSArray *)args
{
    [self openDevice:[args realObjectAtIndex:0]
               block:[args realObjectAtIndex:1]];
}

- (void)closeDevice:(SYSaneDevice *)device
{
    ENSURE_RUNNING_ON_SANE_THREAD(_cmd, device);
    
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
    
    NSArray *args = [NSArray arrayWithNullableObjects:2, device, [block copy]];
    ENSURE_RUNNING_ON_SANE_THREAD(@selector(listOptionsForDevice:), args);
    
    PERF_START();
    
    NSValue *value = self.openedDevices[device.name];
    SANE_Handle h = [value pointerValue];
    
    SANE_Int count;
    NSMutableArray *opts = [NSMutableArray array];
    
#warning check why unused value
    const SANE_Option_Descriptor *dn = sane_get_option_descriptor(h, 0);
    sane_control_option(h, 0, SANE_ACTION_GET_VALUE, &count, NULL);
    
    for(uint i = 1; i < count; ++i)
    {
        dn = sane_get_option_descriptor(h, i);
        [opts addObject:[SYSaneOption initWithCOpt:dn index:i device:device]];
    }
    
    for(SYSaneOption *d in opts)
    {
        [d refreshValue:nil];
    }
    
    PERF_END(PERF_MIN_MS);
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [device setGroupedOptions:[SYSaneOption groupedElements:opts]];
        if(block)
            block();
    });
}

- (void)listOptionsForDevice:(NSArray *)args
{
    [self listOptionsForDevice:[args realObjectAtIndex:0]
                         block:[args realObjectAtIndex:1]];
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
    
    NSArray *args = [NSArray arrayWithNullableObjects:2, option, [block copy]];
    ENSURE_RUNNING_ON_SANE_THREAD(@selector(getValueForOptionOnDevice:), args);

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

- (void)getValueForOptionOnDevice:(NSArray *)args
{
    [self getValueForOption:[args realObjectAtIndex:0]
                      block:[args realObjectAtIndex:1]];
}

- (void)setValue:(id)value
     orAutoValue:(BOOL)autoValue
       forOption:(SYSaneOption *)option
 thenReloadValue:(BOOL)reloadValue
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
    
    NSArray *args = [NSArray arrayWithNullableObjects:5, value, @(autoValue), option, @(reloadValue), [block copy]];
    ENSURE_RUNNING_ON_SANE_THREAD(@selector(setValueForOptionOnDevice:), args);
    
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
    
    PERF_START();
    SANE_Int info;
    SANE_Status s;
    if (autoValue)
        s = sane_control_option(h, option.index, SANE_ACTION_SET_AUTO, byteValue, &info);
    else
        s = sane_control_option(h, option.index, SANE_ACTION_SET_VALUE, byteValue, &info);
    
    BOOL reloadAllOptions = (info & SANE_INFO_RELOAD_OPTIONS || info & SANE_INFO_RELOAD_PARAMS);
    PERF_END(PERF_MIN_MS);

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
    
    if (reloadValue && !reloadAllOptions)
        [option refreshValue:nil];

    dispatch_async(dispatch_get_main_queue(), ^{
        if (block)
            block(reloadAllOptions, nil);
    });
}

- (void)setValueForOptionOnDevice:(NSArray *)args
{
    [self setValue:[args realObjectAtIndex:0]
       orAutoValue:[[args realObjectAtIndex:1] boolValue]
         forOption:[args realObjectAtIndex:2]
   thenReloadValue:[[args realObjectAtIndex:3] boolValue]
             block:[args realObjectAtIndex:4]];
}

- (void)scanWithDevice:(SYSaneDevice *)device
         progressBlock:(void (^)(float))progressBlock
          successBlock:(void (^)(UIImage *, NSString *))successBlock
{
    if(![self isDeviceOpen:device]) {
        if (successBlock)
            successBlock(nil, @"Device not opened");
        return;
    }
    
    NSArray *args = [NSArray arrayWithNullableObjects:3, device, [progressBlock copy], [successBlock copy]];
    ENSURE_RUNNING_ON_SANE_THREAD(@selector(scanWithDevice:), args);
    
    NSValue *handle = self.openedDevices[device.name];
    SANE_Handle h = [handle pointerValue];
    
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
    SANE_Byte *bytes = malloc(bufferMaxSize);
    SANE_Int bufferActualSize = 0;
    SYSaneScanParameters *parameters = 0;
    while (s == SANE_STATUS_GOOD)
    {
        if (progressBlock && parameters.fileSize) {
            dispatch_async(dispatch_get_main_queue(), ^{
                progressBlock((double)data.length / (double)parameters.fileSize);
            });
        }
        
        [data appendData:[NSData dataWithBytes:bytes length:bufferActualSize]];
        s = sane_read(h, bytes, bufferMaxSize, &bufferActualSize);
        
        if (!parameters)
        {
            SANE_Parameters params;
            sane_get_parameters(h, &params);
            parameters = [[SYSaneScanParameters alloc] initWithCParams:params];
            NSLog(@"%@", parameters);
        }
    }
    
    free(bytes);

    if(s != SANE_STATUS_EOF)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (successBlock)
                successBlock(nil, status);
        });
        return;
    }

    
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *error;
        UIImage *image = [UIImage imageFromRGBData:data saneParameters:parameters error:&error];
        
        if (successBlock)
            successBlock(image, error);
    });
}

- (void)scanWithDevice:(NSArray *)args
{
    [self scanWithDevice:[args realObjectAtIndex:0]
           progressBlock:[args realObjectAtIndex:1]
            successBlock:[args realObjectAtIndex:2]];
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
