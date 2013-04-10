/*  ****** NOTE ******
 *  This header file should be the LAST header file included within every
 *  .c file within the project.   If there are .h files that have actual
 *  code in them, then this header should be the last include within that
 *  .h file, and that .h file should be the last one included within the
 *  .c file.
 *  ****** NOTE *****
 */
#if !defined (__MEM_DBG_H_)
#define __MEM_DBG_H_

// values to use within the MemDbg_Validate() function.
#define MEMDBG_VALIDATE_MIN     0
#define MEMDBG_VALIDATE_DEEP    1
#define MEMDBG_VALIDATE_DEEPER  2
#define MEMDBG_VALIDATE_DEEPEST 3

/*
 * within the next included file, there is a single line that will turn on or off ALL
 * malloc debugging. There are other defines that will increase the level of debugging
 * to check for more things.  By default, there will be no malloc debugging.
 */
#include "memdbg_defines.h"
#if defined (MEMDBG_ON)

/*
 * This software was written by Jim Fougeron jfoug AT cox dot net
 * in 2013. No copyright is claimed, and the software is hereby
 * placed in the public domain. In case this attempt to disclaim
 * copyright and place the software in the public domain is deemed
 * null and void, then the software is Copyright (c) 2013 Jim Fougeron
 * and it is hereby released to the general public under the following
 * terms:
 *
 * This software may be modified, redistributed, and used for any
 * purpose, in source and binary forms, with or without modification.
 */

/*
 *  memdbg.h
 *  Memory management debugging (at runtime)
 *
 *   memdbg contains routines detect, and report memory
 *   problems, such as double frees, passing bad pointers to
 *   free, most buffer overwrites.  Also, tracking of non-freed
 *   data, showing memory leaks, can also be shown.
 *
 *  Compilation Options (in the memdbg_defines.h file)
 *
 *   MEMDBG_ON     If this is NOT defined, then memdbg will
 *       get out of your way, and most normal memory functions
 *       will be called with no overhead at all.
 */

#include <stdio.h>
#include <string.h>

/* these functions can be called by client code. Normally Memdbg_Used() and
 * MemDbg_Display() would be called at program exit. That will dump a list
 * of any memory that was not released.  The MemDbg_Validate() can be called
 * pretty much any time.  That function will walk the memory allocation linked
 * lists, and sqwack if there are problems, such as overwrites, freed memory that
 * has been written to, etc.  It would likely be good to call MemDbg_Validate()
 * within benchmarking, after every format is tested.
 *
 *  TODO:  Add a handle that can be passed to the MemDbg_Used() and MemDbg_Display()
 *  and a function to get the 'current' state of memory as a handle.  Thus, a
 *  format self test could get a handle BEFORE starting, and then check after, and
 *  ONLY show leaked memory from the time the handle was obtained, which was at the
 *  start of the self test. Thus it would only show leaks from that format test.
 *
 *  These functions are NOT thread safe. Do not call them within OMP blocks of code.
 *  Normally, these would be called at program exit, or within things like format
 *  self test code, etc, and not within OMP.  But this warning is here, so that
 *  it is known NOT to call within OMP.
 */
extern unsigned long MemDbg_Used(int show_freed);
extern void          MemDbg_Display(FILE *);
extern void	     MemDbg_Validate(int level);
extern void	     MemDbg_Validate_msg(int level, const char *pMsg);
extern void	     MemDbg_Validate_msg2(int level, const char *pMsg, int bShowExData);

/* these functions should almost NEVER be called by any client code. They
 * are listed here, because the macros need to know their names. Client code
 * should almost ALWAYS call malloc() like normal, vs calling MEMDBG_alloc()
 * If MEMDBG_alloc() was called, and MEMDBG_ON was not defined, then this
 * function would not be declared here, AND at link time, the function would
 * not be found.
 * NOTE, these functions should be thread safe in OMP builds (using #pragma omp atomic)
 * also note, memory allocation within OMP blocks SHOULD be avoided if possible. It is
 * very slow, and the thread safety required makes it even slow. This is not only talking
 * about these functions here, BUT malloc/free in general in OMP blocks. AVOID doing that
 * at almost all costs, and performance will usually go up.
 */
extern void *MEMDBG_alloc(size_t, char *, int);
extern void *MEMDBG_realloc(const void *, size_t, char *, int);
extern void MEMDBG_free(const void *, char *, int);
extern char *MEMDBG_strdup(const char *, char *, int);

#if !defined(__MEMDBG__)
/* we get here on every file compiled EXCEPT memdbg.c */
#undef malloc
#undef realloc
#undef free
#undef strdup
#undef libc_free
#define libc_free(a)    do {if(a) MEMDBG_libc_free(a); a=0; } while(0)
#define malloc(a)     MEMDBG_alloc((a),__FILE__,__LINE__)
#define realloc(a,b)  MEMDBG_realloc((a),(b),__FILE__,__LINE__)
/* this code mimicks JtR's FREE_MEM(a) but does it for any MEMDBG_free(a,F,L) call (a hooked free(a) call) */
#define free(a)       do { if (a) MEMDBG_free((a),__FILE__,__LINE__); a=0; } while(0)
#define strdup(a)     MEMDBG_strdup((a),__FILE__,__LINE__)
#endif

/* pass the file handle to write to (normally stderr) */
#define MEMDBG_PROGRAM_EXIT_CHECKS(a) do { \
    if (MemDbg_Used(0) > 0) MemDbg_Display(a); \
    MemDbg_Validate_msg2(MEMDBG_VALIDATE_DEEPEST, "At Program Exit", 1); } while(0)

#else
/* NOTE, we DO keep one special function here.  We make free a little
 * smarter. this function gets used, even when we do NOT compile with
 * any memory debugging on. This makes free work more like C++ delete,
 * in that it is valid to call it on a NULL. Also, it sets the pointer
 * to NULL, so that we can call free(x) on x multiple times, without
 * causing a crash. NOTE, the multiple frees SHOULD be caught when
 * someone builds and runs with MEMDBG_ON. But when it is off, we do
 * try to protect the program.
 */
#undef libc_free
#define libc_free(a)  do {if(a) MEMDBG_libc_free(a); a=0; } while(0)
#if !defined(__MEMDBG__)
/* this code mimicks JtR's FREE_MEM(a) but does it for any normal free(a) call */
extern void MEMDBG_off_free(void *a);
#define free(a)   do { if(a) MEMDBG_off_free(a); a=0; } while(0)
#endif
#define MemDbg_Used(a) 0
#define MemDbg_Display(a)
#define MemDbg_Validate(a)
#define MemDbg_Validate_msg(a,b)
#define MemDbg_Validate_msg2(a,b,c)
#define MEMDBG_PROGRAM_EXIT_CHECKS(a)
#endif /* MEMDBG_ON */

extern void MEMDBG_libc_free(void *);

#endif /* __MEMDBG_H_ */
