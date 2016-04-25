//
//  SYGridLayout.h
//  SaneScanner
//
//  Created by Stan Chevallier on 24/04/2016.
//  Copyright © 2016 Syan. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface SYGridLayout : UICollectionViewFlowLayout

@property (nonatomic, assign) CGFloat maxSize;
@property (nonatomic, assign) CGFloat margin;

@property (nonatomic) CGFloat minimumLineSpacing      NS_UNAVAILABLE;
@property (nonatomic) CGFloat minimumInteritemSpacing NS_UNAVAILABLE;
@property (nonatomic) CGSize itemSize                 NS_UNAVAILABLE;
@property (nonatomic) CGSize estimatedItemSize        NS_UNAVAILABLE;

@end
