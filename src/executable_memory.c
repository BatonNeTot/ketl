//🫖ketl
#include "executable_memory.h"

#include <string.h>

#if ANN_OS_LINUX

#include <unistd.h>
#include <sys/mman.h>

inline static uint32_t ketl_get_page_size(void) {
	return getpagesize();
}

inline static void* ketl_allocate_exe_memory(void* p_mem_hint, uint32_t size) {
	return mmap(p_mem_hint, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

inline static void ketl_deallocate_exe_memory(void* ptr, uint32_t size) {
	munmap(ptr, size);
}

inline static void ketl_protect_exe_memory(void* ptr, uint32_t size) {
	mprotect(ptr, size, PROT_READ | PROT_EXEC);
}

inline static void ketl_unprotect_exe_memory(void* ptr, uint32_t size) {
	mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

#endif


#if ANN_OS_WINDOWS
#include <Windows.h>

inline static uint32_t ketl_get_page_size(void) {
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwPageSize;
}

inline static void* ketl_allocate_exe_memory(void* p_mem_hint, uint32_t size) {
	// hinting is not an option in windows
	// I could specify an implicit address, but not currently possible alongside random malloc
	(void)p_mem_hint; 
	return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
}

inline static void ketl_deallocate_exe_memory(void* ptr, uint32_t size) {
	(void)size;
	VirtualFree(ptr, 0, MEM_RELEASE);
}

inline static void ketl_protect_exe_memory(void* ptr, uint32_t size) {
	DWORD dummy;
	VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &dummy);
}

inline static void ketl_unprotect_exe_memory(void* ptr, uint32_t size) {
	DWORD dummy;
	VirtualProtect(ptr, size, PAGE_READWRITE, &dummy);
}

#endif

KETL_VECTOR_DEFINITION(ketl_executable_memory_page_vector, ketl_executable_memory_page)

static uint32_t ketl_get_static_page_size(void) {
	static uint32_t page_size = 0;
	if (page_size == 0) {
		page_size = ketl_get_page_size();
	}
	return page_size;
}

static uint32_t ketl_get_static_page_size_log(void) {
	static uint32_t page_size_log = -1;
	if (page_size_log == (uint32_t)(-1)) {
		uint32_t page_size = ketl_get_page_size();
		while (page_size > 0) {
			page_size >>= 1;
			++page_size_log;
		}
	}
	return page_size_log;
}

void ketl_executable_memory_init(ketl_executable_memory* exe_memory, const ketl_allocator* p_allocator) {
	*exe_memory = (ketl_executable_memory) {
		.current_offset = 0,
	};
	ketl_executable_memory_page_vector_init(&exe_memory->v_pages, 1, p_allocator);
}

void ketl_executable_memory_deinit(ketl_executable_memory* exe_memory) {
	ketl_executable_memory_page_vector v_pages = exe_memory->v_pages;
	for (uint32_t i = 0u; i < v_pages.size; ++i) {
		ketl_executable_memory_page page = v_pages.p_data[i];
		ketl_deallocate_exe_memory(page.p_page, page.page_size);
	}

	ketl_executable_memory_page_vector_deinit(&v_pages);
}

uint8_t* ketl_executable_memory_allocate(ketl_executable_memory* exe_memory, const uint8_t* opcodes, uint64_t length) {
	if (length == 0) {
		return NULL;
	}

	uint32_t current_offset = exe_memory->current_offset;
	ketl_executable_memory_page current_page;
	if (exe_memory->v_pages.size == 0 || current_offset + length > current_page.page_size) {
		uint32_t page_size = ketl_get_static_page_size();
		uint32_t requested_page_count = (uint32_t)((length + (page_size - 1)) >> ketl_get_static_page_size_log());

		void* p_mem_hint = NULL;
		if (exe_memory->v_pages.size != 0) {
			p_mem_hint = current_page.p_page + current_page.page_size;
		}

		current_page.page_size = page_size * requested_page_count;
		current_page.p_page = ketl_allocate_exe_memory(p_mem_hint, current_page.page_size);
		
		ketl_executable_memory_page_vector_push_back_ref(&exe_memory->v_pages, &current_page);
		current_offset = 0;
	} else {
		current_page = exe_memory->v_pages.p_data[exe_memory->v_pages.size - 1];
		ketl_unprotect_exe_memory(current_page.p_page, current_page.page_size);
	}

	uint32_t requested_memory = (length + 15) & (-16);
	uint8_t* result_memory = current_page.p_page + current_offset;

	memcpy(result_memory, opcodes, length);
	exe_memory->current_offset = current_offset + requested_memory;

	ketl_protect_exe_memory(current_page.p_page, current_page.page_size);

	return result_memory;
}
