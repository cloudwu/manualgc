/*
 *	Copyright (c) 2008 , 
 *		Cloud Wu . All rights reserved.
 *
 *		http://www.codingnow.com
 *
 *	Use, modification and distribution are subject to the "New BSD License"
 *	as listed at <url: http://www.opensource.org/licenses/bsd-license.php >.
 */

#ifndef MANUAL_GARBAGE_COLLECTOR_H
#define MANUAL_GARBAGE_COLLECTOR_H

#include <stddef.h>

void gc_init();
void gc_exit();

void gc_enter();
void gc_leave(void *p,...);

void* gc_malloc(size_t sz,void *parent,void (*finalization)(void *));
void* gc_realloc(void *p,size_t sz,void *parent);
void gc_link(void *parent,void *prev,void *now);
void gc_collect();

void* gc_clone(void *from,size_t sz);
void* gc_weak(void *p);

void gc_dryrun();

#endif
