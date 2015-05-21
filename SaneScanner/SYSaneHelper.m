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
#import "SYSaneOptionDescriptor.h"
#import "sane.h"

#define ON_MAIN_THREAD (1)

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

void sane_auth(SANE_String_Const resource, SANE_Char *username, SANE_Char *password)
{
    NSLog(@"auth needed! %s", resource);
    username = "yo";
    password = "yo";
}

@interface SYSaneHelper ()
@property (atomic, strong) NSThread *thread;
@property (atomic, strong) NSMutableArray *hosts;
@property (atomic, strong) NSArray *devices;
@property (atomic, strong) NSDictionary *openedDevices;
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
        
        NSArray *savedHosts = [NSArray arrayWithContentsOfFile:[SYTools hostsFile]];
        self.hosts = [NSMutableArray arrayWithArray:savedHosts];
        
#if !ON_MAIN_THREAD
        [self performSelector:@selector(initSane) onThread:self.thread withObject:nil waitUntilDone:YES];
#endif
        [self updateDevices];
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

- (NSArray *)hostsCopy
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

- (NSArray *)devicesCopy
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
            if(isUpdatingDevices && self.startingDevicesUpdateBlock)
                self.startingDevicesUpdateBlock();
            if(!isUpdatingDevices && self.endedDevicesUpdateBlock)
                self.endedDevicesUpdateBlock();
        });
    }
}

- (void)updateDevices
{
    ENSURE_RUNNING_ON_SANE_THREAD(nil, nil);
    
    if(self.isUpdatingDevices)
        return;
    
    self.isUpdatingDevices = YES;
    
    // refreshing without restarting sane first causes lots of crashs
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
    SANE_Status s = sane_open([device.name cStringUsingEncoding:NSUTF8StringEncoding], &h);
    
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
    [self openDevice:args[0] block:([args count] == 2 ? args[1] : nil)];
}

- (void)closeDevice:(SYSaneDevice *)device
{
    ENSURE_RUNNING_ON_SANE_THREAD(_cmd, device);
    
    if(![[self.openedDevices allKeys] containsObject:device.name])
        return;
    
    NSValue *value = self.openedDevices[device.name];
    SANE_Handle h = [value pointerValue];
    sane_close(h);

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

#pragma mark - Options

- (void)listOptionsForDevice:(SYSaneDevice *)device block:(void(^)(void))block
{
    if(![self isDeviceOpen:device])
        return;
    
    NSArray *args = block ? @[device, block] : @[device];
    ENSURE_RUNNING_ON_SANE_THREAD(@selector(listOptionsForDevice:), args);
    
    NSValue *value = self.openedDevices[device.name];
    SANE_Handle h = [value pointerValue];
    
    SANE_Int count;
    NSMutableArray *opts = [NSMutableArray array];
    
    const SANE_Option_Descriptor *dn = sane_get_option_descriptor(h, 0);
    sane_control_option(h, 0, SANE_ACTION_GET_VALUE, &count, NULL);
    
    for(uint i = 1; i < count; ++i)
    {
        dn = sane_get_option_descriptor(h, i);
        [opts addObject:[[SYSaneOptionDescriptor alloc] initWithCOpt:dn index:i device:device]];
    }
    
    for(SYSaneOptionDescriptor *d in opts)
    {
        [d updateValue:nil];
    }
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [device setGroupedOptions:[SYSaneOptionDescriptor groupedElements:opts]];
        if(block)
            block();
    });
}

- (void)listOptionsForDevice:(NSArray *)args
{
    [self listOptionsForDevice:args[0] block:([args count] == 2 ? args[1] : nil)];
}

- (void)getValueForOption:(SYSaneOptionDescriptor *)option
                 onDevice:(SYSaneDevice *)device
                    block:(void(^)(id value, NSString *error))block
{
    if(![self isDeviceOpen:device]) {
        block(nil, @"Device not opened");
        return;
    }
    
    if(option.type == SANE_TYPE_GROUP) {
        block(nil, @"No value for type GROUP");
        return;
    }
    
    if(option.type == SANE_TYPE_BUTTON) {
        block(nil, @"No value for type BUTTON");
        return;
    }
    
    NSArray *args = block ? @[option, device, block] : @[option, device];
    ENSURE_RUNNING_ON_SANE_THREAD(@selector(getValueForOptionOnDevice:), args);
    
    NSValue *handle = self.openedDevices[device.name];
    SANE_Handle h = [handle pointerValue];
    
    void *value = malloc(option.size);
    SANE_Status s = sane_control_option(h, option.index, SANE_ACTION_GET_VALUE, value, NULL);

    if(s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        dispatch_async(dispatch_get_main_queue(), ^{
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
        block(castedValue, nil);
    });
}

- (void)getValueForOptionOnDevice:(NSArray *)args
{
    [self getValueForOption:args[0] onDevice:args[1] block:([args count] > 2 ? args[2] : nil)];
}

- (void)scanWithDevice:(SYSaneDevice *)device block:(void(^)(UIImage *image, NSString *error))block
{
    
}


@end
