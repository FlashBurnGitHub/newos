/*
** Copyright 2004 Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <unistd.h>
#include <errno.h>
#include <sys/syscalls.h>

pid_t 
getsid(pid_t pid)
{
	// XXX implement
	return 1;
//	return _kern_setpgid((proc_id)pid, (pgrp_id)pgid);
}
