/*
** Copyright 2002-2004, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <sys/atomic.h>
#include <sys/syscalls.h>

int atomic_add(int *val, int incr)
{
	return _kern_atomic_add(val, incr);
}

int atomic_and(int *val, int incr)
{
	return _kern_atomic_and(val, incr);
}

int atomic_or(int *val, int incr)
{
	return _kern_atomic_or(val, incr);
}

int atomic_set(int *val, int set_to)
{
	return _kern_atomic_set(val, set_to);
}

int test_and_set(int *val, int set_to, int test_val)
{
	return _kern_test_and_set(val, set_to, test_val);
}

