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
#import "saneopts.h"
#import "UIImage+SY.h"
#import "NSMutableData+SY.h"
#import <NSObject+SYKit.h>
#import "SYPreferences.h"
#import "SYGettextTranslation.h"

extern int sanei_debug_net;

#define ENSURE_RUNNING_THREAD(thread, block) \
if ([NSThread currentThread] != (thread)) { [self sy_performBlock:(block) onThread:(thread)]; return; }

#define ENSURE_RUNNING_ON_SANE_THREAD(block) ENSURE_RUNNING_THREAD(self.thread, block)

#define RUN_BLOCK_ON_THREAD(useMain, block, ...) \
if (!block) return; else if (useMain) {dispatch_sync(dispatch_get_main_queue(), ^{block(__VA_ARGS__);});} else block(__VA_ARGS__)

#define PERF_START()    \
NSDate *perfDate = [NSDate date]

#define PERF_END(min)   \
if ([[NSDate date] timeIntervalSinceDate:perfDate] > ((double)min)/1000.) \
    NSLog($$("%@: %.03lfs"), NSStringFromSelector(_cmd), [[NSDate date] timeIntervalSinceDate:perfDate])

#define PERF_MIN_MS (1000)

static NSString * const SYSaneHelper_PrefKey_Hosts = $$("hosts");
static NSString * const SYSaneHelper_ConfFileName  = $$("dll.conf");
static NSString * const SYSaneHelper_NetFileName   = $$("net.conf");

void sane_auth(SANE_String_Const resource, SANE_Char *username, SANE_Char *password);

@interface SYSaneHelper ()
@property (atomic, strong) NSThread *thread;
@property (atomic, strong) NSMutableArray <NSString *> *hosts;
@property (atomic, strong) NSDictionary <NSString *, NSValue *> *openedDevices;
@property (atomic, strong) SYGettextTranslation *gettextTranslation;
@property (atomic, strong) NSLock *lockIsUpdating;
@property (atomic, assign) BOOL saneStarted;
@property (atomic, assign) BOOL stopScanOperation;
@end

@implementation SYSaneHelper

@synthesize isUpdatingDevices = _isUpdatingDevices;

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
        [self loadTranslationFile];
        
        NSArray <NSString *> *savedHosts = [[NSUserDefaults standardUserDefaults] arrayForKey:SYSaneHelper_PrefKey_Hosts];
        self.hosts = [NSMutableArray arrayWithArray:savedHosts];
        [self commitHosts];
        
        NSLog($$("Loaded hosts: %@"), [self.hosts componentsJoinedByString:$$(", ")]);
        
        self.thread = [[NSThread alloc] initWithTarget:self selector:@selector(threadKeepAlive) object:nil];
        [self.thread setName:$$("SYSaneHelper")];
        [self.thread start];
        
        self.lockIsUpdating = [[NSLock alloc] init];
        
        [self sy_performBlock:^{ [self startSaneIfNeeded]; } onThread:self.thread];
    }
    return self;
}

- (void)dealloc
{
    sane_exit();
}

- (void)startSaneIfNeeded
{
    ENSURE_RUNNING_THREAD(self.thread, ^{
        [self startSaneIfNeeded];
    });
    
    if (self.saneStarted)
        return;
    
    // needed for Sane-net config file
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[SYTools appSupportPath:YES]];
    
    // needed for dll.conf file
    NSData *dllConf = [NSData dataWithContentsOfFile:SYSaneHelper_ConfFileName];
    [dllConf writeToFile:[SYTools appSupportPath:NO] atomically:YES];
    
    self.openedDevices = [NSMutableDictionary dictionary];
    self.isUpdatingDevices = NO;
    
    SANE_Status s = sane_init(NULL, sane_auth);
    sanei_debug_net = 10;

    if (s != SANE_STATUS_GOOD)
    {
        NSString *status = [NSString stringWithCString:sane_strstatus(s) encoding:NSUTF8StringEncoding];
        self.saneInitError = status;
    }
    
    self.saneStarted = (s == SANE_STATUS_GOOD);
    NSLog($$("Sane started: %d"), self.saneStarted);
}

- (void)stopSane
{
    ENSURE_RUNNING_THREAD(self.thread, ^{
        [self stopSane];
    });
    
    if (!self.saneStarted)
        return;
    
    self.openedDevices = nil;
    sane_exit();
    
    self.saneStarted = NO;
    NSLog($$("Sane stopped"));
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

#pragma mark - Translation

- (void)loadTranslationFile
{
    NSString *translationFilenameFormat = $$("translations/sane_strings_%@.po");
    NSArray <NSString *> *localeIDs = @[[NSLocale currentLocale].localeIdentifier,
                                        [[NSLocale currentLocale] objectForKey:NSLocaleLanguageCode]];
    
    for (NSString *localeID in localeIDs)
    {
        NSString *translationFilename = [NSString stringWithFormat:translationFilenameFormat, localeID];
        NSString *translationPath = [[NSBundle mainBundle] pathForResource:translationFilename ofType:nil];
        if ([[NSFileManager defaultManager] fileExistsAtPath:translationPath])
        {
            self.gettextTranslation = [SYGettextTranslation gettextTranslationWithContentsOfFile:translationPath];
            break;
        }
    }
}

- (NSString *)translationForKey:(NSString *)key
{
    return ([self.gettextTranslation translationForKey:key] ?: key);
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
    [self updateDevices:nil];
}

- (void)removeHost:(NSString *)host
{
    if (!host.length)
        return;
    
    [self.hosts removeObject:host];
    [self commitHosts];
    [self updateDevices:nil];
}

- (void)commitHosts
{
    [[NSUserDefaults standardUserDefaults] setObject:self.hosts.copy forKey:SYSaneHelper_PrefKey_Hosts];
    
    NSMutableArray <NSString *> *lines = [NSMutableArray array];
    [lines addObjectsFromArray:self.hosts];
    [lines addObject:$$("connect_timeout = 10")];
    NSString *config = [lines componentsJoinedByString:$$("\n")];
    
    NSLog($$("Updating config to:\n%@"), config);
    
    NSString *configPath = [[SYTools appSupportPath:YES] stringByAppendingPathComponent:SYSaneHelper_NetFileName];
    [config writeToFile:configPath atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    
    [[NSFileManager defaultManager] setAttributes:@{} ofItemAtPath:config error:NULL];
}

#pragma mark - Devices

- (BOOL)isUpdatingDevices
{
    BOOL updating;
    [self.lockIsUpdating lock];
    updating = self->_isUpdatingDevices;
    [self.lockIsUpdating unlock];
    return updating;
}

- (void)setIsUpdatingDevices:(BOOL)isUpdatingDevices
{
    [self.lockIsUpdating lock];
    self->_isUpdatingDevices = isUpdatingDevices;
    [self.lockIsUpdating unlock];
    
    dispatch_sync(dispatch_get_main_queue(), ^
    {
        if (isUpdatingDevices)
            [self.delegate saneHelperDidStartUpdatingDevices:self];
        if (!isUpdatingDevices)
            [self.delegate saneHelperDidEndUpdatingDevices:self];
    });
}

- (void)updateDevices:(void(^)(NSArray <SYSaneDevice *> *devices, NSError *error))block
{
    if (self.isUpdatingDevices)
        return;
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self updateDevices:block];
    });
    
    [self startSaneIfNeeded];
    
    self.isUpdatingDevices = YES;
    
    const SANE_Device **devices = NULL;
    
    PERF_START();
    SANE_Status s = sane_get_devices(&devices, SANE_FALSE);
    PERF_END(PERF_MIN_MS);
    
    if (s != SANE_STATUS_GOOD)
    {
        RUN_BLOCK_ON_THREAD(YES, block, nil, [NSError sy_errorWithSaneStatus:s]);
        self.isUpdatingDevices = NO;
        return;
    }
    
    NSMutableArray <SYSaneDevice *> *devicesObjects = [NSMutableArray array];
    
    uint i = 0;
    while (devices[i])
    {
        [devicesObjects addObject:[[SYSaneDevice alloc] initWithCDevice:devices[i]]];
        ++i;
    }

    self.isUpdatingDevices = NO;
    
    RUN_BLOCK_ON_THREAD(YES, block, [devicesObjects copy], nil);
    
    [self stopSane];
}

- (void)openDevice:(SYSaneDevice *)device block:(void(^)(NSError *error))block
{
    [self openDevice:device block:block useMainThread:[NSThread isMainThread]];
}

- (void)openDevice:(SYSaneDevice *)device block:(void(^)(NSError *error))block useMainThread:(BOOL)useMainThread
{
    if (!device)
        return;
    
    if ([[self.openedDevices allKeys] containsObject:device.name])
        return;
    
    [self startSaneIfNeeded];
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self openDevice:device block:block useMainThread:useMainThread];
    });
    
    SANE_Handle h = NULL;
    
    PERF_START();
    SANE_Status s = sane_open([device.name cStringUsingEncoding:NSUTF8StringEncoding], &h);
    PERF_END(PERF_MIN_MS);
    
    if (s != SANE_STATUS_GOOD)
    {
        RUN_BLOCK_ON_THREAD(useMainThread, block, [NSError sy_errorWithSaneStatus:s]);
        return;
    }
    
    NSMutableDictionary *openedDevices = [NSMutableDictionary dictionaryWithDictionary:self.openedDevices];
    [openedDevices setObject:[NSValue valueWithPointer:h] forKey:device.name];
    self.openedDevices = [openedDevices copy];

    RUN_BLOCK_ON_THREAD(useMainThread, block, nil);
}

- (void)closeDevice:(SYSaneDevice *)device
{
    [self closeDeviceWithName:device.name];
}

- (void)closeDeviceWithName:(NSString *)name
{
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self closeDeviceWithName:name];
    });
    
    if (![[self.openedDevices allKeys] containsObject:name])
        return;
    
    NSValue *value = self.openedDevices[name];
    SANE_Handle h = [value pointerValue];
    PERF_START();
    sane_close(h);
    PERF_END(PERF_MIN_MS);

    NSMutableDictionary *openedDevices = [NSMutableDictionary dictionaryWithDictionary:self.openedDevices];
    [openedDevices removeObjectForKey:name];
    self.openedDevices = [openedDevices copy];
    
    if (!self.openedDevices.count)
        [self stopSane];
}

- (BOOL)isDeviceOpen:(SYSaneDevice *)device
{
    if (!device)
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
    [self listOptionsForDevice:device block:block useMainThread:[NSThread isMainThread]];
}

- (void)listOptionsForDevice:(SYSaneDevice *)device block:(void(^)(void))block useMainThread:(BOOL)useMainThread
{
    if (![self isDeviceOpen:device])
        return;
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self listOptionsForDevice:device block:block useMainThread:useMainThread];
    });
    
    PERF_START();
    
    NSValue *value = self.openedDevices[device.name];
    SANE_Handle h = [value pointerValue];
    
    SANE_Int count;
    
    // needed for sane to update the value of the option count
    const SANE_Option_Descriptor *descriptor = sane_get_option_descriptor(h, 0);
    
    SANE_Status s = sane_control_option(h, 0, SANE_ACTION_GET_VALUE, &count, NULL);
    if (s != SANE_STATUS_GOOD)
    {
        RUN_BLOCK_ON_THREAD(useMainThread, block);
        return;
    }
    
    NSMutableArray *opts = [NSMutableArray array];
    for (uint i = 1; i < count; ++i)
    {
        descriptor = sane_get_option_descriptor(h, i);
        [opts addObject:[SYSaneOption initWithCOpt:descriptor index:i device:device]];
    }
    
    for (SYSaneOption *d in opts)
        [d refreshValue:nil];
    
    PERF_END(PERF_MIN_MS);
    
    [device setOptions:[opts copy]];
    RUN_BLOCK_ON_THREAD(useMainThread, block);
}

- (void)getValueForOption:(SYSaneOption *)option
                    block:(void(^)(id value, NSError *error))block
{
    [self getValueForOption:option block:block useMainThread:[NSThread isMainThread]];
}

- (void)getValueForOption:(SYSaneOption *)option
                    block:(void(^)(id value, NSError *error))block
            useMainThread:(BOOL)useMainThread
{
    if (![self isDeviceOpen:option.device]) {
        if (block)
            block(nil, [NSError sy_errorWithCode:SYErrorCode_DeviceNotOpened]);
        return;
    }
    
    if (option.type == SANE_TYPE_GROUP) {
        if (block)
            block(nil, [NSError sy_errorWithCode:SYErrorCode_GetValueForTypeGroup]);
        return;
    }
    
    if (option.type == SANE_TYPE_BUTTON) {
        if (block)
            block(nil, [NSError sy_errorWithCode:SYErrorCode_GetValueForTypeButton]);
        return;
    }
    
    if (option.capInactive) {
        if (block)
            block(nil, [NSError sy_errorWithCode:SYErrorCode_GetValueForInactiveOption]);
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self getValueForOption:option block:block useMainThread:useMainThread];
    });

    NSValue *handle = self.openedDevices[option.device.name];
    SANE_Handle h = [handle pointerValue];
    
    void *value = malloc(option.size);
    PERF_START();
    SANE_Status s = sane_control_option(h, option.index, SANE_ACTION_GET_VALUE, value, NULL);
    PERF_END(PERF_MIN_MS);

    if (s != SANE_STATUS_GOOD)
    {
        RUN_BLOCK_ON_THREAD(useMainThread, block, nil, [NSError sy_errorWithSaneStatus:s]);
        return;
    }
    
    id castedValue;
    if (option.type == SANE_TYPE_BOOL) {
        SANE_Bool *bValue = value;
        castedValue = [NSNumber numberWithBool:bValue[0]];
    }
    if (option.type == SANE_TYPE_INT || option.type == SANE_TYPE_FIXED) {
        SANE_Int *iValue = value;
        castedValue = [NSNumber numberWithInt:iValue[0]];
    }
    if (option.type == SANE_TYPE_STRING) {
        SANE_String sValue = value;
        castedValue = [NSString stringWithCString:sValue encoding:NSUTF8StringEncoding];
    }
    
    free(value);
    
    RUN_BLOCK_ON_THREAD(useMainThread, block, castedValue, nil);
}

- (void)setCropArea:(CGRect)cropArea
            useAuto:(BOOL)useAuto
          forDevice:(SYSaneDevice *)device
              block:(void(^)(BOOL reloadAllOptions, NSError *error))block
      useMainThread:(BOOL)useMainThread
{
    if (![self isDeviceOpen:device]) {
        if (block)
            block(NO, [NSError sy_errorWithCode:SYErrorCode_DeviceNotOpened]);
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self setCropArea:cropArea useAuto:useAuto forDevice:device block:block useMainThread:useMainThread];
    });
    
    __block BOOL finalReloadAllOptions = NO;
    __block NSError *finalError;
    
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
        [self setValue:values[i] orAutoValue:NO forOption:option block:^(BOOL reloadAllOptions, NSError *error) {
            finalReloadAllOptions = finalReloadAllOptions || reloadAllOptions;
            finalError = error;
        }];
        
        if (finalError)
        {
            RUN_BLOCK_ON_THREAD(useMainThread, block, finalReloadAllOptions, finalError);
            return;
        }
    }
    
    RUN_BLOCK_ON_THREAD(useMainThread, block, finalReloadAllOptions, finalError);
}

- (void)setValue:(id)value
     orAutoValue:(BOOL)autoValue
       forOption:(SYSaneOption *)option
           block:(void(^)(BOOL reloadAllOptions, NSError *error))block
{
    [self setValue:value orAutoValue:autoValue forOption:option block:block useMainThread:[NSThread isMainThread]];
}

- (void)setValue:(id)value
     orAutoValue:(BOOL)autoValue
       forOption:(SYSaneOption *)option
           block:(void(^)(BOOL reloadAllOptions, NSError *error))block
   useMainThread:(BOOL)useMainThread
{
    if (![self isDeviceOpen:option.device]) {
        if (block)
            block(NO, [NSError sy_errorWithCode:SYErrorCode_DeviceNotOpened]);
        return;
    }
    
    if (option.type == SANE_TYPE_GROUP) {
        if (block)
            block(NO, [NSError sy_errorWithCode:SYErrorCode_SetValueForTypeGroup]);
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self setValue:value orAutoValue:autoValue forOption:option block:block useMainThread:useMainThread];
    });
    
    NSValue *handle = self.openedDevices[option.device.name];
    SANE_Handle h = [handle pointerValue];
    
    void *byteValue = NULL;
    
    if (!autoValue) {
        if (option.type == SANE_TYPE_BOOL) {
            byteValue = malloc(option.size);
            ((SANE_Bool *)byteValue)[0] = [value boolValue];
        }
        else if (option.type == SANE_TYPE_INT) {
            byteValue = malloc(option.size);
            ((SANE_Int *)byteValue)[0] = [value intValue];
        }
        else if (option.type == SANE_TYPE_FIXED) {
            byteValue = malloc(option.size);
            ((SANE_Fixed *)byteValue)[0] = SANE_FIX([value doubleValue]);
        }
        else if (option.type == SANE_TYPE_STRING) {
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
    
    BOOL reloadAllOptions = (info & SANE_INFO_RELOAD_OPTIONS || info & SANE_INFO_RELOAD_PARAMS);
    BOOL updatedValue = NO;
    
    if (s == SANE_STATUS_GOOD && !autoValue)
    {
        if (option.type == SANE_TYPE_BOOL) {
            SYSaneOptionBool *castedOption = (SYSaneOptionBool *)option;
            [castedOption setValue:((SANE_Bool *)byteValue)[0]];
            updatedValue = YES;
        }
        else if (option.type == SANE_TYPE_INT) {
            SYSaneOptionNumber *castedOption = (SYSaneOptionNumber *)option;
            [castedOption setValue:@(((SANE_Int *)byteValue)[0])];
            updatedValue = YES;
        }
        else if (option.type == SANE_TYPE_FIXED) {
            SYSaneOptionNumber *castedOption = (SYSaneOptionNumber *)option;
            [castedOption setValue:@(SANE_UNFIX(((SANE_Fixed *)byteValue)[0]))];
            updatedValue = YES;
        }
        else if (option.type == SANE_TYPE_STRING) {
            SYSaneOptionString *castedOption = (SYSaneOptionString *)option;
            [castedOption setValue:[NSString stringWithCString:(SANE_String)byteValue encoding:NSUTF8StringEncoding]];
            updatedValue = YES;
        }
    }
    
    if (!autoValue && (option.type == SANE_TYPE_BOOL || option.type == SANE_TYPE_INT || option.type == SANE_TYPE_FIXED))
        free(byteValue);
    
    if (s != SANE_STATUS_GOOD)
    {
        RUN_BLOCK_ON_THREAD(useMainThread, block, NO, [NSError sy_errorWithSaneStatus:s]);
        return;
    }
    
    if (!reloadAllOptions &&!updatedValue)
        [option refreshValue:nil];
    
    RUN_BLOCK_ON_THREAD(useMainThread, block, reloadAllOptions, nil);
}

- (void)previewWithDevice:(SYSaneDevice *)device
            progressBlock:(void (^)(float, UIImage *))progressBlock
             successBlock:(void (^)(UIImage *, NSError *))successBlock
{
    [self previewWithDevice:device progressBlock:progressBlock successBlock:successBlock useMainThread:[NSThread isMainThread]];
}

- (void)previewWithDevice:(SYSaneDevice *)device
            progressBlock:(void (^)(float, UIImage *))progressBlock
             successBlock:(void (^)(UIImage *, NSError *))successBlock
            useMainThread:(BOOL)useMainThread
{
    if (![self isDeviceOpen:device]) {
        if (successBlock)
            successBlock(nil, [NSError sy_errorWithCode:SYErrorCode_DeviceNotOpened]);
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self previewWithDevice:device progressBlock:progressBlock successBlock:successBlock useMainThread:useMainThread];
    });
    
    NSMutableDictionary <NSNumber *, id> *oldOptions = [NSMutableDictionary dictionary];
    SYSaneOption *optionPreview = [device standardOption:SYSaneStandardOptionPreview];
    
    if (optionPreview)
    {
        [self setValue:@(YES) orAutoValue:NO forOption:optionPreview block:nil];
    }
    else
    {
        NSArray <NSNumber *> *stdOptions = @[@(SYSaneStandardOptionResolution),
                                             @(SYSaneStandardOptionResolutionX),
                                             @(SYSaneStandardOptionResolutionY),
                                             @(SYSaneStandardOptionAreaTopLeftX),
                                             @(SYSaneStandardOptionAreaTopLeftY),
                                             @(SYSaneStandardOptionAreaBottomRightX),
                                             @(SYSaneStandardOptionAreaBottomRightY)];
        
        if ([[SYPreferences shared] previewWithAutoColorMode])
            stdOptions = [stdOptions arrayByAddingObject:@(SYSaneStandardOptionColorMode)];
        
        for (NSNumber *stdOptionN in stdOptions)
        {
            SYSaneStandardOption stdOption = stdOptionN.unsignedIntegerValue;
            SYSaneOption *option = [device standardOption:stdOption];
            SYOptionValue bestValue = SYBestValueForPreviewValueForOption(stdOption);
            
            if (!option)
                continue;
            
            id newValue = nil;
            BOOL useAuto = NO;
            
            if ([option isKindOfClass:[SYSaneOptionNumber class]])
            {
                SYSaneOptionNumber *castedOption = (SYSaneOptionNumber *)option;
                
                newValue = castedOption.bestValueForPreview;
                useAuto  = (bestValue == SYOptionValueAuto);
                [oldOptions setObject:castedOption.value forKey:@(stdOption)];
            }
            else if ([option isKindOfClass:[SYSaneOptionString class]])
            {
                SYSaneOptionString *castedOption = (SYSaneOptionString *)option;
                
                if (bestValue != SYOptionValueAuto)
                {
                    NSLog($$("Unsupported configuration : option %@ is a string but cannot be set to auto"), option.name);
                    continue;
                }
                
                useAuto = YES;
                [oldOptions setObject:castedOption.value forKey:@(stdOption)];
            }
            else
            {
                NSLog($$("Unsupported configuration : option type for %@ is not supported"), option.name);
            }
            
            [self setValue:newValue orAutoValue:useAuto forOption:option block:^(BOOL reloadAllOptions, NSError *error) {
                if (reloadAllOptions)
                    [[SYSaneHelper shared] listOptionsForDevice:device block:nil useMainThread:NO];
            } useMainThread:NO];
        }
    }
    
    __block UIImage *previewImage;
    __block NSError *previewError;
    
    [self scanWithDevice:device
         useScanCropArea:NO
           progressBlock:progressBlock
            successBlock:^(UIImage *image, NSError *error)
    {
        previewImage = image;
        previewError = error;
    } useMainThread:NO];
    
    if (optionPreview)
    {
        [self setValue:@(NO) orAutoValue:NO forOption:optionPreview block:nil];
    }
    else
    {
        for (NSNumber *stdOptionN in oldOptions.allKeys)
        {
            SYSaneStandardOption stdOption = stdOptionN.unsignedIntegerValue;
            SYSaneOption *option = [device standardOption:stdOption];
            
            [self setValue:oldOptions[stdOptionN] orAutoValue:NO forOption:option block:^(BOOL reloadAllOptions, NSError *error) {
                if (reloadAllOptions)
                    [[SYSaneHelper shared] listOptionsForDevice:device block:nil useMainThread:NO];
            } useMainThread:NO];
        }
    }
    
    [device setLastPreviewImage:previewImage];
    RUN_BLOCK_ON_THREAD(useMainThread, successBlock, previewImage, previewError);
}

- (void)scanWithDevice:(SYSaneDevice *)device
         progressBlock:(void (^)(float, UIImage *))progressBlock
          successBlock:(void (^)(UIImage *, NSError *))successBlock
{
    [self scanWithDevice:device
         useScanCropArea:YES
           progressBlock:progressBlock
            successBlock:successBlock
           useMainThread:[NSThread isMainThread]];
}

- (void)scanWithDevice:(SYSaneDevice *)device
       useScanCropArea:(BOOL)useScanCropArea
         progressBlock:(void (^)(float, UIImage *))progressBlock
          successBlock:(void (^)(UIImage *, NSError *))successBlock
         useMainThread:(BOOL)useMainThread
{
    if (![self isDeviceOpen:device]) {
        if (successBlock)
            successBlock(nil, [NSError sy_errorWithCode:SYErrorCode_DeviceNotOpened]);
        return;
    }
    
    ENSURE_RUNNING_ON_SANE_THREAD(^{
        [self scanWithDevice:device
             useScanCropArea:useScanCropArea
               progressBlock:progressBlock
                successBlock:successBlock
               useMainThread:useMainThread];
    });
    
    self.stopScanOperation = NO;
    
    NSValue *handle = self.openedDevices[device.name];
    SANE_Handle h = [handle pointerValue];
    
    if (device.canCrop && useScanCropArea)
        [self setCropArea:device.cropArea useAuto:NO forDevice:device block:nil useMainThread:useMainThread];
    
    PERF_START();
    SANE_Status s = sane_start(h);
    PERF_END(PERF_MIN_MS);
    
    if (s != SANE_STATUS_GOOD)
    {
        RUN_BLOCK_ON_THREAD(useMainThread, successBlock, nil, [NSError sy_errorWithSaneStatus:s]);
        return;
    }
    
    s = sane_set_io_mode(h, NO);
    if (s != SANE_STATUS_GOOD)
    {
        RUN_BLOCK_ON_THREAD(useMainThread, successBlock, nil, [NSError sy_errorWithSaneStatus:s]);
        return;
    }
    
    SANE_Parameters estimatedParams;
    sane_get_parameters(h, &estimatedParams);
    SYSaneScanParameters *estimatedParameters = [[SYSaneScanParameters alloc] initWithCParams:estimatedParams];
    
    // TODO: handle big scans gracefully, for instance scan to file directly, and generate image only if device seems capable
    
    //NSString *filename = [[SYTools documentsPath] stringByAppendingPathComponent:$$("scan.tmp")];
    //NSFileHandle *fileHandle = [NSFileHandle fileHandleForWritingAtPath:filename];
    
    NSMutableData *data = [NSMutableData dataWithCapacity:estimatedParameters.fileSize];
    SANE_Int bufferMaxSize = MAX(100*1000, (int)estimatedParameters.fileSize / 100);
    SANE_Byte *buffer = malloc(bufferMaxSize);
    SANE_Int bufferActualSize = 0;
    
    SYSaneScanParameters *parameters;
    
    int previousLogLevel = sanei_debug_net;
    sanei_debug_net = 0;
    
    // generate incomplete image preview every 2%
    float progressForLastIncompletePreview = 0.;
    float incompletePreviewStep = 0.02;
    
    while (s == SANE_STATUS_GOOD)
    {
        if (progressBlock && parameters.fileSize)
        {
            float progress = (double)data.length / (double)parameters.fileSize;
            if ([[SYPreferences shared] showIncompleteScanImages] &&
                progress > progressForLastIncompletePreview + incompletePreviewStep)
            {
                progressForLastIncompletePreview = progress;
                
                dispatch_sync(dispatch_get_main_queue(), ^{
                    // image creation needs to be done on main thread
                    UIImage *incompleteImage = [UIImage imageFromIncompleteRGBData:[data copy] saneParameters:parameters error:NULL];
                    if (progressBlock)
                        progressBlock(progress, incompleteImage);
                });
            }
            else
                RUN_BLOCK_ON_THREAD(YES, progressBlock, progress, nil);
        }
        
        [data appendData:[NSData dataWithBytes:buffer length:bufferActualSize]];
        //[fileHandle writeData:[NSData dataWithBytes:buffer length:bufferActualSize]];
        
        if (self.stopScanOperation)
        {
            self.stopScanOperation = NO;
            sane_cancel(h);
        }
        
        s = sane_read(h, buffer, bufferMaxSize, &bufferActualSize);
        
        if (!parameters)
        {
            SANE_Parameters params;
            sane_get_parameters(h, &params);
            parameters = [[SYSaneScanParameters alloc] initWithCParams:params];
        }

        // lineart requires inverting pixel values
        if (parameters.currentlyAcquiredChannel == SANE_FRAME_GRAY && parameters.depth == 1)
        {
            for (int i = 0; i < bufferActualSize; ++i)
                buffer[i] = ~buffer[i];
        }
    }
    
    free(buffer);
    
    sanei_debug_net = previousLogLevel;

    if (s != SANE_STATUS_EOF)
    {
        RUN_BLOCK_ON_THREAD(useMainThread, successBlock, nil, [NSError sy_errorWithSaneStatus:s]);
        return;
    }

    NSError *error;
    UIImage *image = [UIImage imageFromRGBData:data saneParameters:parameters error:&error];
    
    RUN_BLOCK_ON_THREAD(useMainThread, successBlock, image, error);
}

- (void)stopCurrentScan
{
    self.stopScanOperation = YES;
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
