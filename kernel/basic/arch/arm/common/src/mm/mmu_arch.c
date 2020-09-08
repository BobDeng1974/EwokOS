#include <mm/mmu.h>
#include <mm/kalloc.h>
#include <kstring.h>
#include <stddef.h>
#include <kernel/system.h>
#include <kernel/kernel.h>

//static inline void __attribute__((optimize("O0"))) set_extra_flags(page_table_entry_t* pte, uint32_t is_dev) {
static inline void set_extra_flags(page_table_entry_t* pte, uint32_t is_dev) {
	pte->bufferable = 0;
	pte->cacheable = 0;

#ifdef A_CORE
	if(is_dev == 0) { //normal mem
		//pte->tex = 0x1;
		pte->cacheable = 1;
		//pte->bufferable = 1;
	}
	//else { //device 
		//pte->tex = 0x7;
	//}
#else
	(void)is_dev;
	if(pte->ap == AP_RW_RW) {
		pte->tex = 0x7;
		pte->apx = 1;
		pte->ng = 1;
	}
	else {
		pte->tex = 0x5;
		pte->apx = 0;
	}
	pte->sharable = 1;
#endif
}

/*
 * map_page adds to the given virtual memory the mapping of a single virtual page
 * to a physical page.
 * Notice: virtual and physical address inputed must be all aliend by PAGE_SIZE !
 */
//int32_t __attribute__((optimize("O0"))) map_page(page_dir_entry_t *vm, uint32_t virtual_addr,
int32_t map_page(page_dir_entry_t *vm, uint32_t virtual_addr,
		     uint32_t physical, uint32_t permissions, uint32_t is_dev) {
	page_table_entry_t *page_table = 0;

	uint32_t page_dir_index = PAGE_DIR_INDEX(virtual_addr);
	uint32_t page_index = PAGE_INDEX(virtual_addr);

	/* if this page_dirEntry is not mapped before, map it to a new page table */
	if (vm[page_dir_index].type == 0) {
		page_table = kalloc1k();
		if(page_table == NULL)
			return -1;

		memset(page_table, 0, PAGE_TABLE_SIZE);
		vm[page_dir_index].type = PAGE_DIR_2LEVEL_TYPE;
		vm[page_dir_index].sbz= 0;
		vm[page_dir_index].domain = 0;
		vm[page_dir_index].base = PAGE_TABLE_TO_BASE(V2P(page_table));
	}
	/* otherwise use the previously allocated page table */
	else {
		page_table = (void *) P2V(BASE_TO_PAGE_TABLE(vm[page_dir_index].base));
	}

	/* map the virtual page to physical page in page table */
	page_table[page_index].type = SMALL_PAGE_TYPE,
	page_table[page_index].base = PAGE_TO_BASE(physical);
	page_table[page_index].ap = permissions;
	set_extra_flags(&page_table[page_index], is_dev);
	return 0;
}

/* unmap_page clears the mapping for the given virtual address */
void unmap_page(page_dir_entry_t *vm, uint32_t virtual_addr) {
	page_table_entry_t *page_table = 0;
	uint32_t page_dir_index = PAGE_DIR_INDEX(virtual_addr);
	uint32_t page_index = PAGE_INDEX(virtual_addr);
	page_table = (void *) P2V(BASE_TO_PAGE_TABLE(vm[page_dir_index].base));
	page_table[page_index].type = 0;
}

/*
 * resolve_physical_address simulates the virtual memory hardware and maps the
 * given virtual address to physical address. This function can be used for
 * debugging if given virtual memory is constructed correctly.
 */
uint32_t resolve_phy_address(page_dir_entry_t *vm, uint32_t virtual) {
	page_dir_entry_t *pdir = 0;
	page_table_entry_t *page = 0;
	uint32_t result = 0;
	uint32_t base_address = 0;

	pdir = (page_dir_entry_t*)((uint32_t) vm | ((virtual >> 20) << 2));
	base_address = pdir->base << 10;
	page = (page_table_entry_t*)((uint32_t) base_address | ((virtual >> 10) & 0x3fc));
	page = (page_table_entry_t*)P2V(page);
	result = (page->base << 12) | (virtual & 0xfff);
	return result;
}

/*
get page entry(virtual addr) by virtual address
*/
page_table_entry_t* get_page_table_entry(page_dir_entry_t *vm, uint32_t virtual) {
	page_dir_entry_t *pdir = 0;
	page_table_entry_t *page = 0;
	uint32_t base_address = 0;

	pdir = (void *) ((uint32_t) vm | ((virtual >> 20) << 2));
	base_address = pdir->base << 10;
	page = (page_table_entry_t*)((uint32_t) base_address | ((virtual >> 10) & 0x3fc));
	page = (page_table_entry_t*)P2V(page);
	return page;
}
