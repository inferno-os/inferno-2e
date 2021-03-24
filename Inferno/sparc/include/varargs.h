/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _VARARGS_H
#define	_VARARGS_H

#pragma ident	"@(#)varargs.h	1.18	94/01/27 SMI"	/* SVr4.0 1.4.1.5 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Many compilation systems depend upon the use of special functions
 * built into the the compilation system to handle variable argument
 * lists and stack allocations.  The method to obtain this in SunOS
 * is to define the feature test macro "__BUILTIN_VA_ARG_INCR" which
 * enables the following special built-in functions:
 *	__builtin_alloca
 *	__builtin_va_alist
 *	__builtin_va_arg_incr
 * It is intended that the compilation system define this feature test
 * macro, not the user of the system.
 *
 * The tests on the processor type are to provide a transitional period
 * for existing compilation systems, and may be removed in a future
 * release.
 */

#ifndef	_VA_LIST
#define	_VA_LIST
typedef char *va_list;
#endif

#if (defined(__BUILTIN_VA_ARG_INCR) || defined(sparc) || defined(__sparc) || \
    defined(i386) || defined(__i386))
#define	va_alist __builtin_va_alist
#endif

#define	va_dcl int va_alist;
#define	va_start(list) list = (char *) &va_alist
#define	va_end(list)

#if (defined(__BUILTIN_VA_ARG_INCR) || defined(sparc) || defined(__sparc) || \
    defined(i386) || defined(__i386)) && !(defined(lint) || defined(__lint))
#define	va_arg(list, mode) ((mode*)__builtin_va_arg_incr((mode *)list))[0]
#else
#define	va_arg(list, mode) ((mode *)(list += sizeof (mode)))[-1]
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _VARARGS_H */
