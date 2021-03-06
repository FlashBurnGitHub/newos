/*	$OpenBSD: exit.c,v 1.4 1997/07/25 18:21:56 mickey Exp $	*/
/*	$NetBSD: exit.c,v 1.11 1996/12/01 20:22:19 pk Exp $	*/

/*-
 *  Copyright (c) 1993 John Brezak
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef __STDC__
#include <machine/stdarg.h>
#else
#include <machine/varargs.h>
#endif

#include "stand.h"

__dead void
#ifdef __STDC__
panic(const char *fmt, ...)
#else
panic(fmt /*, va_alist */)
	char *fmt;
#endif
{
    extern void closeall __P((void));
    va_list ap;
    static int paniced;
    
    if (!paniced) {
        paniced = 1;
        closeall();
    }

#ifdef __STDC__
    va_start(ap, fmt);
#else
    va_start(ap);
#endif
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    _rtt();
    /*NOTREACHED*/
}

void
exit()
{
    panic("exit");
    /*NOTREACHED*/
}

