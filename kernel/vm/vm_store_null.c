/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/lock.h>
#include <kernel/vm_store_vnode.h>
#include <kernel/vfs.h>
#include <sys/errors.h>

static void null_destroy(struct vm_store *store)
{
	if(store) {
		kfree(store);
	}
}

static off_t null_commit(struct vm_store *store, off_t size)
{
	store->committed_size = size;
	return size;
}

static int null_has_page(struct vm_store *store, off_t offset)
{
	return 1; // we always have the page, man
}

static ssize_t null_read(struct vm_store *store, off_t offset, iovecs *vecs)
{
	return -1;
}

static int null_write(struct vm_store *store, off_t offset, iovecs *vecs)
{
	return -1;
}

static int null_fault(struct vm_store *store, struct vm_address_space *aspace, off_t offset)
{
	/* we can't fault on this region, that's pretty much the point of the null store object */
	return ERR_VM_PF_FATAL;
}

static vm_store_ops null_ops = {
	&null_destroy,
	&null_commit,
	&null_has_page,
	&null_read,
	&null_write,
	&null_fault,
	NULL,
	NULL
};

vm_store *vm_store_create_null()
{
	vm_store *store;

	store = kmalloc(sizeof(vm_store));
	if(store == NULL) {
		return NULL;
	}

	store->ops = &null_ops;
	store->cache = NULL;
	store->data = NULL;
	store->committed_size = 0;

	return store;
}
