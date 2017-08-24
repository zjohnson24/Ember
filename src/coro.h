/* Altered from below.
* coroutine.h is copyright 1995,2000 Simon Tatham.
*
* Permission is hereby granted, free of charge, to any person
* obtaining a copy of this software and associated documentation
* files (the "Software"), to deal in the Software without
* restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following
* conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
* NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
* ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
/* int ascending (coro_args) {
*    coro_ctx_start;
*    int i;
*    coro_ctx_stop(foo);
*
*    coro_begin(foo);
*    for (foo->i=0; foo->i<10; foo->i++) {
*       coro_return(foo->i);
*    }
*    coro_finish(-1);
*  }
*/

#ifndef CORO_H
#define CORO_H

#include <stdlib.h>

#if __cplusplus
extern "C" {
#endif

#define coro_args void **coro_params
#define coro_ctx_start   struct coro_ctx { int coro_line
#define coro_ctx_stop(x) } *x = (struct coro_ctx *)*coro_params

#define coro_begin(x) if(!x) {x= *coro_params=malloc(sizeof(*x)); x->coro_line=0;}\
                      if (x) switch(x->coro_line) { case 0:;
#define coro_finish(z)     } free(*coro_params); *coro_params=0; return (z)
#define coro_finish_       } free(*coro_params); *coro_params=0; return

#define coro_return(z)     \
        do {\
            ((struct coro_ctx *)*coro_params)->coro_line=__LINE__;\
            return (z); case __LINE__:;\
        } while (0)
#define coro_return_       \
        do {\
            ((struct coro_ctx *)*coro_params)->coro_line=__LINE__;\
            return; case __LINE__:;\
        } while (0)

#define coro_stop(z) do{ free(*coro_params); *coro_params=0; return (z); }while(0)
#define coro_stop_   do{ free(*coro_params); *coro_params=0; return; }while(0)

#define coro_ctx    void *
#define coro_abort(ctx) do { free (ctx); ctx = 0; } while (0)

typedef struct {
	void(*coro)();
	coro_ctx ctx;
	uint16_t args_len;
	void *args[];
} coro;

#if __cplusplus
}
#endif

#endif
