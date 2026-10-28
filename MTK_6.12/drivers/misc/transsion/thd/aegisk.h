/*Transsion Top Secret*/
/*
 * Copyright (C) 2025 Transsion Inc. All rights reserved.
 *
 * @author chonglin.yang
 */
int startAegisK(const char *name, const char *pids, const char *extra);
int uploadBinderInfo2Aegis(const char *type, const char *binder_info,const char *extra);

extern int aegisk_init(void);
extern int aegisk_exit(void);