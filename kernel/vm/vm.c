#include <kernel/kernel.h>
#include <kernel/vm.h>
#include <kernel/vm_priv.h>
#include <kernel/heap.h>
#include <kernel/debug.h>
#include <kernel/console.h>
#include <kernel/int.h>
#include <kernel/smp.h>
#include <kernel/sem.h>

#include <boot/stage2.h>

#include <kernel/arch/cpu.h>
#include <kernel/arch/pmap.h>
#include <kernel/arch/vm.h>

#include <libc/string.h>
#include <libc/ctype.h>
#include <libc/printf.h>

static unsigned int first_free_page_index = 0;
static unsigned int *free_page_table = NULL;
static unsigned int free_page_table_base = 0;
static unsigned int free_page_table_size = 0;

#define END_OF_LIST 0xffffffff
#define PAGE_INUSE  0xfffffffe

static void dump_free_page_table(int argc, char **argv);

#define HEAP_SIZE	0x00400000

static struct aspace *aspace_list = NULL;
static struct aspace *kernel_aspace = NULL;
static aspace_id next_aspace_id = 0;

static area_id next_area_id = 0;

struct area *vm_find_area_by_name(struct aspace *aspace, const char *name)
{
	struct area *area;

	area = aspace->area_list;
	while(area != NULL) {
		if(strcmp(area->name, name) == 0)
			return area;
		area = area->next;
	}
	return NULL;
}

struct area *_vm_create_area_struct(struct aspace *aspace, char *name, unsigned int base, unsigned int size, unsigned int lock)
{
	struct area *area;
	struct area *a, *last = NULL;
	
	// allocate an area struct to represent this area
	area = (struct area *)kmalloc(sizeof(struct area));	
	area->base = base;
	area->size = size;
	area->id = next_area_id++;
	area->lock = lock;
	area->name = (char *)kmalloc(strlen(name) + 1);
	strcpy(area->name, name);
	
	// insert into the list
	// we'll need to search for the spot
	// check for address overlaps
	a = aspace->area_list;
	while(a != NULL) {
//		dprintf("create_area_struct: a = 0x%x. base = 0x%x, size = 0x%x\n", a, a->base, a->size);
		if(a->base > base) {
			if(base + size > a->base) {
				// overlap
				kfree(area->name);
				kfree(area);
				return NULL;
			}
			area->next = a;
			if(last == NULL)
				aspace->area_list = area;
			else
				last->next = area;
			break;
		}
		last = a;
		a = a->next;	
	}
	if(a == NULL) {
		area->next = NULL;
		if(last == NULL)
			aspace->area_list = area;
		else
			last->next = area;
	}
	aspace->area_count++;

	return area;
}

enum {
	SRC_ADDR_ANY = 0,
	SRC_ADDR_PHYSICAL,
	SRC_ADDR_MAPPED_ALREADY,
	SRC_ADDR_CONTIGUOUS
};

static area_id _vm_create_area(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock,
	unsigned int src_addr, unsigned int src_addr_type)
{
	struct area *area;
	unsigned int base;

	dprintf("_vm_create_area: '%s', *addr = 0x%p, addr_type = %d, size = %d, src_addr = 0x%x\n",
		name, *addr, addr_type, size, src_addr);

	// check validity of lock
	if((lock & ~LOCK_MASK) != 0) {
		// invalid lock
		panic("_vm_create_area called with invalid lock %d\n", lock);
	}
	
	switch(addr_type) {
		case AREA_ANY_ADDRESS: {
			struct area *a;
			struct area *next_a = NULL;
			// find a hole big enough for a new area
	
			base = 0;
	
			a = aspace->area_list;
			if(a == NULL) {
				base = aspace->base;
			} else if(a != NULL && a->base > aspace->base) {
				// lets try to build the area at the beginning of the aspace
				if(aspace->base + size > a->base) {
					// if we built it here, it would overlap, so let the loop below
					// find the right spot
					next_a = a->next;
				} else {
					// otherwise, we're done.
					base = aspace->base;
					a = NULL;
				}
			} else {
				next_a = a->next;
			}
			while(a != NULL) {
//				dprintf("a = 0x%x. base = 0x%x, size = 0x%x\n", a, a->base, a->size);
				if(next_a != NULL) {
//					dprintf("next_a = 0x%x. base = 0x%x, size = 0x%x\n", next_a, next_a->base, next_a->size);
					if(next_a->base - (a->base + a->size) >= size) {
						// we have a spot
						base = a->base + a->size;
						break;
					}
				} else {
					if((aspace->base + aspace->size) - (a->base + a->size) + 1 >= size) {
						// we have a spot
						base = a->base + a->size;
						break;
					}
				}
				a = next_a;
				if(next_a != NULL)
					next_a = next_a->next;
			}
	
			if(base == 0)
				return -1;
			*addr = (void *)base;
			area = _vm_create_area_struct(aspace, name, base, size, lock);
			break;
		}
		case AREA_EXACT_ADDRESS:
			base = (unsigned int)*addr;
			
			if(base < aspace->base || (base - aspace->base) + size > (base - aspace->base) + aspace->size) {
				dprintf("_vm_create_area: area asked to be created outside of aspace\n");
				return -1;
			}
			
			area = _vm_create_area_struct(aspace, name, base, size, lock);
			break;
		default:
			// whut?
			area = NULL;
			return -1;
	}

	// ok, now we've allocated the area descriptor and put it in place,
	// lets find the pages
	switch(src_addr_type) {
		case SRC_ADDR_ANY: {
			unsigned int i;
			unsigned int page;

			for(i=0; i < PAGE_ALIGN(size) / PAGE_SIZE; i++) {
				vm_get_free_page(&page);
				pmap_map_page(page * PAGE_SIZE, base + i * PAGE_SIZE, lock); 
			}
			break;
		}
		case SRC_ADDR_CONTIGUOUS: {
			unsigned int i;
			unsigned int page;

			if(vm_get_free_page_run(&page, PAGE_ALIGN(size) / PAGE_SIZE) < 0) {
				panic("_vm_create_area: asked for contiguous area of len %d, could not find it\n", size);
			}

			for(i=0; i < PAGE_ALIGN(size) / PAGE_SIZE; i++) {
				pmap_map_page((page + i) * PAGE_SIZE, base + i * PAGE_SIZE, lock);
			}
			break;
		}
		case SRC_ADDR_PHYSICAL: {
			unsigned int i;
			
			vm_mark_page_range_inuse(src_addr / PAGE_SIZE, PAGE_ALIGN(size) / PAGE_SIZE);
			for(i=0; i < PAGE_ALIGN(size) / PAGE_SIZE; i++) {
				pmap_map_page(src_addr + i * PAGE_SIZE, base + i * PAGE_SIZE, lock); 
			}
			break;
		}
		case SRC_ADDR_MAPPED_ALREADY:
			break;
		default:
			// hmmm
			return -1;
	}
	
	return area->id;
}

area_id vm_create_area(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock, int flags)
{
	if(addr_type == AREA_ALREADY_MAPPED) {
		return _vm_create_area(aspace, name, addr, AREA_EXACT_ADDRESS, size, lock, 0, SRC_ADDR_MAPPED_ALREADY);
	} else {
		if(flags == AREA_FLAGS_CONTIG)
			return _vm_create_area(aspace, name, addr, addr_type, size, lock, 0, SRC_ADDR_CONTIGUOUS);
		else
			return _vm_create_area(aspace, name, addr, addr_type, size, lock, 0, 0);
	}

}

int vm_map_physical_memory(struct aspace *aspace, char *name, void **addr, int addr_type,
	unsigned int size, unsigned int lock, unsigned int phys_addr)
{
	return _vm_create_area(aspace, name, addr, addr_type, size, lock, phys_addr, SRC_ADDR_PHYSICAL);
}

int vm_get_page_mapping(addr vaddr, addr *paddr)
{
	return pmap_get_page_mapping(vaddr, paddr);
}

static void display_mem(int argc, char **argv)
{
	int item_size;
	int display_width;
	int num = 1;
	addr address;
	int i;
	int j;
	
	if(argc < 2) {
		dprintf("not enough arguments\n");
		return;
	}
	
	address = atoul(argv[1]);
	
	if(argc >= 3) {
		num = -1;
		num = atoi(argv[2]);
	}

	// build the format string
	if(strcmp(argv[0], "db") == 0) {
		item_size = 1;
		display_width = 16;
	} else if(strcmp(argv[0], "ds") == 0) {
		item_size = 2;
		display_width = 8;
	} else if(strcmp(argv[0], "dw") == 0) {
		item_size = 4;
		display_width = 4;
	} else {
		dprintf("display_mem called in an invalid way!\n");
		return;
	}

	dprintf("[0x%x] '", address);
	for(j=0; j<min(display_width, num) * item_size; j++) {
		char c = *((char *)address + j);
		if(!isalnum(c)) {
			c = '.';
		}
		dprintf("%c", c);
	}
	dprintf("'");
	for(i=0; i<num; i++) {	
		if((i % display_width) == 0 && i != 0) {
			dprintf("\n[0x%x] '", address + i * item_size);
			for(j=0; j<min(display_width, (num-i)) * item_size; j++) {
				char c = *((char *)address + i * item_size + j);
				if(!isalnum(c)) {
					c = '.';
				}
				dprintf("%c", c);
			}
			dprintf("'");
		}
		
		switch(item_size) {
			case 1:
				dprintf(" 0x%02x", *((uint8 *)address + i));
				break;
			case 2:
				dprintf(" 0x%04x", *((uint16 *)address + i));
				break;
			case 4:
				dprintf(" 0x%08x", *((uint32 *)address + i));
				break;
			default:
				dprintf("huh?\n");
		}
	}
	dprintf("\n");
}

static void vm_dump_kspace_areas(int argc, char **argv)
{
	vm_dump_areas(vm_get_kernel_aspace());
}

void vm_dump_areas(struct aspace *aspace)
{
	struct area *area;

	dprintf("area dump of address space '%s', base 0x%x, size 0x%x:\n", aspace->name, aspace->base, aspace->size);

	for(area = aspace->area_list; area != NULL; area = area->next) {
		dprintf("area 0x%x: ", area->id);
		dprintf("base_addr = 0x%x ", area->base);
		dprintf("size = 0x%x ", area->size);
		dprintf("name = '%s' ", area->name);
		dprintf("lock = 0x%x\n", area->lock);
	}
}

static void dump_aspace_areas(int argc, char **argv)
{
	int id = -1;
	unsigned long num;
	struct aspace *as;

	if(argc < 2) {
		dprintf("aspace_areas: not enough arguments\n");
		return;
	}

	// if the argument looks like a hex number, treat it as such
	if(strlen(argv[1]) > 2 && argv[1][0] == '0' && argv[1][1] == 'x') {
		num = atoul(argv[1]);
		id = num;
	}

	for(as = aspace_list; as != NULL; as = as->next) {
		if(as->id == id || !strcmp(argv[1], as->name)) {
			vm_dump_areas(as);
			break;
		}
	}
}

static void dump_aspace_list(int argc, char **argv)
{
	struct aspace *as;

	dprintf("id\t%32s\tbase\tsize\t\tarea_count\n", "name");
	for(as = aspace_list; as != NULL; as = as->next) {
		dprintf("0x%x\t%32s\t0x%x\t0x%x\t0x%x\n",
			as->id, as->name, as->base, as->size, as->area_count);
	}
}

struct aspace *vm_get_kernel_aspace()
{
	return kernel_aspace;
}

struct aspace *vm_create_aspace(const char *name, unsigned int base, unsigned int size)
{
	struct aspace *aspace;
	
	aspace = (struct aspace *)kmalloc(sizeof(struct aspace));
	if(aspace == NULL)
		return NULL;
	aspace->id = next_aspace_id++;
	aspace->name = (char *)kmalloc(strlen(name) + 1);
	if(aspace->name == NULL ) {
		kfree(aspace);
		return NULL;
	}
	strcpy(aspace->name, name);
	aspace->base = base;
	aspace->size = size;
	aspace->area_list = NULL;
	aspace->area_count = 0;

	// insert it into the aspace list
	aspace->next = aspace_list;
	aspace_list = aspace;

	return aspace;	
}

static addr _alloc_vspace_from_ka_struct(kernel_args *ka, unsigned int size)
{
	addr spot = 0;
	unsigned int i;
	int last_valloc_entry = 0;

	size = PAGE_ALIGN(size);
	// find a slot in the virtual allocation addr range
	for(i=1; i<ka->num_virt_alloc_ranges; i++) {
		last_valloc_entry = i;
		// check to see if the space between this one and the last is big enough
		if(ka->virt_alloc_range[i].start - 
			(ka->virt_alloc_range[i-1].start + ka->virt_alloc_range[i-1].size) >= size) {

			spot = ka->virt_alloc_range[i-1].start + ka->virt_alloc_range[i-1].size;
			ka->virt_alloc_range[i-1].size += size;
			goto out;
		}
	}
	if(spot == 0) {
		// we hadn't found one between allocation ranges. this is ok.
		// see if there's a gap after the last one
		if(ka->virt_alloc_range[last_valloc_entry].start + ka->virt_alloc_range[last_valloc_entry].size + size <=
			KERNEL_BASE + (KERNEL_SIZE - 1)) {
			spot = ka->virt_alloc_range[last_valloc_entry].start + ka->virt_alloc_range[last_valloc_entry].size;
			ka->virt_alloc_range[last_valloc_entry].size += size;
			goto out;
		}
		// see if there's a gap before the first one
		if(ka->virt_alloc_range[0].start > KERNEL_BASE) {
			if(ka->virt_alloc_range[0].start - KERNEL_BASE >= size) {
				ka->virt_alloc_range[0].start -= size;
				spot = ka->virt_alloc_range[0].start;
				goto out;
			}
		}
	}

out:
	return spot;
}

// XXX horrible brute-force method of determining if the page can be allocated
static bool is_page_in_phys_range(kernel_args *ka, addr paddr)
{
	unsigned int i;

	for(i=0; i<ka->num_phys_mem_ranges; i++) {
		if(paddr >= ka->phys_mem_range[i].start &&
			paddr < ka->phys_mem_range[i].start + ka->phys_mem_range[i].size) {
			return true;
		}	
	}
	return false;
}

static addr _alloc_ppage_from_kernel_struct(kernel_args *ka)
{
	unsigned int i;

	for(i=0; i<ka->num_phys_alloc_ranges; i++) {
		addr next_page;

		next_page = ka->phys_alloc_range[i].start + ka->phys_alloc_range[i].size;
		// see if the page after the next allocated paddr run can be allocated
		if(i + 1 < ka->num_phys_alloc_ranges && ka->phys_alloc_range[i+1].size != 0) {
			// see if the next page will collide with the next allocated range
			if(next_page >= ka->phys_alloc_range[i+1].start)
				continue;
		}
		// see if the next physical page fits in the memory block
		if(is_page_in_phys_range(ka, next_page)) {
			// we got one!
			ka->phys_alloc_range[i].size += PAGE_SIZE;
			return (ka->phys_alloc_range[i].start + ka->phys_alloc_range[i].size - PAGE_SIZE);
		}
	}

	return 0;	// could not allocate a block
}

static addr alloc_from_ka_struct(kernel_args *ka, unsigned int size, int lock)
{
	addr vspot;
	addr pspot;
	unsigned int i;
	int curr_phys_alloc_range = 0;

	// find the vaddr to allocate at
	vspot = _alloc_vspace_from_ka_struct(ka, size);
//	dprintf("alloc_from_ka_struct: vaddr 0x%x\n", vspot);

	// map the pages
	for(i=0; i<PAGE_ALIGN(size)/PAGE_SIZE; i++) {
		pspot = _alloc_ppage_from_kernel_struct(ka);
//		dprintf("alloc_from_ka_struct: paddr 0x%x\n", pspot);
		if(pspot == 0)
			panic("error allocating page from ka_struct!\n");
		pmap_map_page(pspot, vspot + i*PAGE_SIZE, lock);
	}
	
	return vspot;
}

int vm_init(kernel_args *ka)
{
	int err = 0;
	unsigned int i;
	int last_used_virt_range = -1;
	int last_used_phys_range = -1;
	addr heap_base;

	dprintf("vm_init: entry\n");
	err = arch_pmap_init(ka);
	err = arch_vm_init(ka);
	
	// calculate the size of memory by looking at the phys_mem_range array
	{
		unsigned int last_phys_page = 0;

		free_page_table_base = ka->phys_mem_range[0].start / PAGE_SIZE;
		for(i=0; i<ka->num_phys_mem_ranges; i++) {
			last_phys_page = (ka->phys_mem_range[i].start + ka->phys_mem_range[i].size) / PAGE_SIZE - 1;
		}
		dprintf("first phys page = 0x%x, last 0x%x\n", free_page_table_base, last_phys_page);
		free_page_table_size = last_phys_page - free_page_table_base;
	}
	
	// map in this new table
	free_page_table = (unsigned int *)alloc_from_ka_struct(ka, free_page_table_size*sizeof(unsigned int),LOCK_KERNEL|LOCK_RW);

	dprintf("vm_init: putting free_page_table @ %p, # ents %d\n",
		free_page_table, free_page_table_size);
	
	// initialize the free page table
	for(i=0; i<free_page_table_size-1; i++) {
		free_page_table[i] = i+1;
	}
	free_page_table[i] = END_OF_LIST;
	first_free_page_index = 0;

	// mark some of the page ranges inuse
	for(i = 0; i < ka->num_phys_alloc_ranges; i++) {
		vm_mark_page_range_inuse(ka->phys_alloc_range[i].start / PAGE_SIZE,
			ka->phys_alloc_range[i].size / PAGE_SIZE);
	}

	// map in the new heap
	heap_base = _alloc_vspace_from_ka_struct(ka, HEAP_SIZE);
	if(heap_base == 0)
		panic("could not allocate heap!\n");
	for(i = 0; i < HEAP_SIZE / PAGE_SIZE; i++) {
		addr ppage;
		if(vm_get_free_page((unsigned int *)&ppage) < 0)
			panic("error getting page for kernel heap!\n");
		pmap_map_page(ppage * PAGE_SIZE, heap_base + i * PAGE_SIZE, LOCK_KERNEL|LOCK_RW);
	}

	heap_init(heap_base, HEAP_SIZE);
	
	// create the initial kernel address space
	kernel_aspace = vm_create_aspace("kernel_land", KERNEL_BASE, KERNEL_SIZE);

	// do any further initialization that the architecture dependant layers may need now
	arch_pmap_init2(ka);
	arch_vm_init2(ka);

	// allocate areas to represent stuff that already exists
	_vm_create_area_struct(kernel_aspace, "kernel_heap", ROUNDOWN((unsigned int)heap_base, PAGE_SIZE), HEAP_SIZE, LOCK_RW|LOCK_KERNEL);
	_vm_create_area_struct(kernel_aspace, "free_page_table", (unsigned int)free_page_table, PAGE_ALIGN(free_page_table_size * sizeof(unsigned int)), LOCK_RW|LOCK_KERNEL);
	_vm_create_area_struct(kernel_aspace, "kernel_seg0", ROUNDOWN((unsigned int)ka->kernel_seg0_addr.start, PAGE_SIZE), PAGE_ALIGN(ka->kernel_seg0_addr.size), LOCK_RW|LOCK_KERNEL);
	_vm_create_area_struct(kernel_aspace, "kernel_seg1", ROUNDOWN((unsigned int)ka->kernel_seg1_addr.start, PAGE_SIZE), PAGE_ALIGN(ka->kernel_seg1_addr.size), LOCK_RW|LOCK_KERNEL);
	for(i=0; i < ka->num_cpus; i++) {
		char temp[64];
		
		sprintf(temp, "idle_thread%d_kstack", i);
		_vm_create_area_struct(kernel_aspace, temp, (unsigned int)ka->cpu_kstack[i].start, ka->cpu_kstack[i].size, LOCK_RW|LOCK_KERNEL);
	}
	{
		void *null;
		vm_map_physical_memory(kernel_aspace, "bootdir", &null, AREA_ANY_ADDRESS,
			ka->bootdir_addr.size, LOCK_RO|LOCK_KERNEL, ka->bootdir_addr.start);
	}
	vm_dump_areas(kernel_aspace);

	// add some debugger commands
	dbg_add_command(&dump_aspace_areas, "aspace_areas", "Dump areas in an address space");
	dbg_add_command(&dump_aspace_list, "aspaces", "Dump a list of all address spaces");
	dbg_add_command(&vm_dump_kspace_areas, "area_dump_kspace", "Dump kernel space areas");
	dbg_add_command(&dump_free_page_table, "free_pages", "Dump free page table list");	
//	dbg_add_command(&display_mem, "dl", "dump memory long words (64-bit)");
	dbg_add_command(&display_mem, "dw", "dump memory words (32-bit)");
	dbg_add_command(&display_mem, "ds", "dump memory shorts (16-bit)");
	dbg_add_command(&display_mem, "db", "dump memory bytes (8-bit)");

	dprintf("vm_init: exit\n");

	return err;
}

int vm_init_postsem(kernel_args *ka)
{
	return heap_init_postsem(ka);
}

int vm_mark_page_range_inuse(unsigned int start_page, unsigned int len)
{
	unsigned int i;
	unsigned int last_i;
	unsigned int j;
	
#if 1
	dprintf("vm_mark_page_range_inuse: entry. start_page %d len %d, first_free %d\n",
		start_page, len, first_free_page_index);
#endif
	dprintf("first_free 0x%x\n", first_free_page_index);
	if(start_page - free_page_table_base || start_page - free_page_table_base + len >= free_page_table_size) {
		dprintf("vm_mark_page_range_inuse: range would extend past free list\n");
		return -1;
	}
	start_page -= free_page_table_base;

	// walk thru the free page list to find the spot
	last_i = END_OF_LIST;
	i = first_free_page_index;
	while(i != END_OF_LIST && i < start_page) {
		last_i = i;
		i = free_page_table[i];
	}

	if(i == END_OF_LIST || i > start_page) {
		dprintf("vm_mark_page_range_inuse: could not find start_page (%d) in free_page_list\n", start_page);
		dump_free_page_table(0, NULL);
		return -1;
	}

	for(j=i; j<(len + i); j++) {
		if(free_page_table[j] == PAGE_INUSE) {
			dprintf("vm_mark_page_range_inuse: found page inuse already\n");	
			dump_free_page_table(0, NULL);
		}
		free_page_table[j] = PAGE_INUSE;
	}

	if(first_free_page_index == i) {
		first_free_page_index = j;
	} else {
		free_page_table[last_i] = j;
	}

	return 0;
}

int vm_mark_page_inuse(unsigned int page)
{
	return vm_mark_page_range_inuse(page, 1);
}

int vm_get_free_page_run(unsigned int *page, unsigned int len)
{
	unsigned int start;
	unsigned int i;
	int err = 0;
	
	start = first_free_page_index;
	if(start == END_OF_LIST)
		return -1;

	for(;;) {
		bool foundit = true;
		if(start + len >= free_page_table_size) {
			err = -1;
			break;
		}
		for(i=0; i<len; i++) {
			if(free_page_table[start + i] != start + i + 1) {
				foundit = false;
				break;
			}
		}
		if(foundit) {
			vm_mark_page_range_inuse(start, len);
			*page = start + free_page_table_base;
			err = 0;
			break;
		} else {
			start = free_page_table[start + i];
			if(start == END_OF_LIST) {
				err = -1;
				break;
			}
		}
	}

	return err;
}

int vm_get_free_page(unsigned int *page)
{
	unsigned int index = first_free_page_index;
	
//	dprintf("vm_get_free_page entry\n");
		
	if(index == END_OF_LIST)
		return -1;

	*page = index + free_page_table_base;
	first_free_page_index = free_page_table[index];
	free_page_table[index] = PAGE_INUSE;
//	dprintf("vm_get_free_page exit, returning page 0x%x\n", *page);
	return 0;
}

static void dump_free_page_table(int argc, char **argv)
{
	unsigned int i = 0;
	unsigned int free_start = END_OF_LIST;
	unsigned int inuse_start = PAGE_INUSE;

	dprintf("dump_free_page_table():\n");
	dprintf("first_free_page_index = %d\n", first_free_page_index);

	while(i < free_page_table_size) {
		if(free_page_table[i] == PAGE_INUSE) {
			if(inuse_start != PAGE_INUSE) {
				i++;
				continue;
			}
			if(free_start != END_OF_LIST) {
				dprintf("free from %d -> %d\n", free_start + free_page_table_base, i-1 + free_page_table_base);
				free_start = END_OF_LIST;
			}
			inuse_start = i;
		} else {
			if(free_start != END_OF_LIST) {
				i++;
				continue;
			}
			if(inuse_start != PAGE_INUSE) {
				dprintf("inuse from %d -> %d\n", inuse_start + free_page_table_base, i-1 + free_page_table_base);
				inuse_start = PAGE_INUSE;
			}
			free_start = i;
		}
		i++;
	}
	if(inuse_start != PAGE_INUSE) {
		dprintf("inuse from %d -> %d\n", inuse_start + free_page_table_base, i-1 + free_page_table_base);
	}
	if(free_start != END_OF_LIST) {
		dprintf("free from %d -> %d\n", free_start + free_page_table_base, i-1 + free_page_table_base);
	}
/*			
	for(i=0; i<free_page_table_size; i++) {
		dprintf("%d->%d ", i, free_page_table[i]);
	}
*/
}

int vm_page_fault(int address, unsigned int fault_address)
{
	dprintf("PAGE FAULT: faulted on address 0x%x. ip = 0x%x. Killing system.\n", address, fault_address);
	
	panic("page fault\n");
//	cli();
	for(;;);
	return INT_NO_RESCHEDULE;
}
