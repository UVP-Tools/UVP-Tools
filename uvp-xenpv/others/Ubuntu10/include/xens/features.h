/******************************************************************************
 * features.h
 *
 * Query the features reported by Xen.
 *
 * Copyright (c) 2006, Ian Campbell
 */

#ifndef __XEN_FEATURES_H__
#define __XEN_FEATURES_H__

#include <xens/interface/features.h>
#include <xens/interface/version.h>

void xen_setup_features(void);

extern u8 xen_features_uvp[XENFEAT_NR_SUBMAPS * 32];

static inline int xen_feature(int flag)
{
	return xen_features_uvp[flag];
}

#endif /* __XEN_FEATURES_H__ */
