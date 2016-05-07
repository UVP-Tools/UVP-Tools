/******************************************************************************
 * features.h
 *
 * Query the features reported by Xen.
 *
 * Copyright (c) 2006, Ian Campbell
 */

#ifndef __FEATURES_H__
#define __FEATURES_H__

#include <interface/features.h>
#include <public/version.h>

extern void setup_xen_features(void);
//void setup_xen_features(void);

extern u8 xen_features[XENFEAT_NR_SUBMAPS * 32];

#define xen_feature(flag)	(xen_features[flag])

#endif /* __ASM_XEN_FEATURES_H__ */
