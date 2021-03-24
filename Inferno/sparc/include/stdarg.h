/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _STDARG_H
#define	_STDARG_H

#ifndef _VA_LIST
#define	_VA_LIST
typedef void *va_list;
#endif

#define	va_start(list, name) (void) (list = (void *)((char *)&...))
#define	va_arg(list, mode) ((mode *)(list = (char *)list + sizeof (mode)))[-1]
extern void va_end(va_list);
#define	va_end(list) (void)0
