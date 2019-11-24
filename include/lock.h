#ifndef __LOCK_H__
#define __LOCK_H__

#define CPU_LOCK() volatile int _flags; DISABLE_INTR(_flags)
#define CPU_UNLOCK() ENABLE_INTR(_flags)

#endif
