//🫖ketl
#include "executable_memory.h"

#include <string.h>

#if KETL_OS_LINUX

#include <unistd.h>
#include <sys/mman.h>

inline static uint32_t ketl_get_page_size() {
	return getpagesize();
}

inline static void* ketl_allocate_exe_memory(void* pMemHint, uint32_t size) {
	return mmap(pMemHint, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
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


#if KETL_OS_WINDOWS
#include <Windows.h>

inline static uint32_t ketl_get_page_size() {
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	return system_info.dwPageSize;
}

inline static void* ketl_allocate_exe_memory(void* pMemHint, uint32_t size) {
	return VirtualAlloc(pMemHint, size, MEM_COMMIT, PAGE_READWRITE);
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

KETL_VECTOR_DEFINITION(ketl_executable_memory_page)

static uint32_t ketl_get_static_page_size() {
	static uint32_t pageSize = 0;
	if (pageSize == 0) {
		pageSize = ketl_get_page_size();
	}
	return pageSize;
}

static uint32_t ketl_get_static_page_size_log() {
	static uint32_t pageSizeLog = -1;
	if (pageSizeLog == (uint32_t)(-1)) {
		uint32_t pageSize = ketl_get_page_size();
		while (pageSize > 0) {
			pageSize >>= 1;
			++pageSizeLog;
		}
	}
	return pageSizeLog;
}

void ketl_executable_memory_init(ketl_executable_memory* exeMemory) {
	*exeMemory = (ketl_executable_memory) {
		.currentOffset = 0,
	};
	ketl_executable_memory_page_vector_init(&exeMemory->vPages, 1);
	exeMemory->vPages.pData[0].pPage = NULL;
}

void ketl_executable_memory_deinit(ketl_executable_memory* exeMemory) {
	ketl_executable_memory_page_vector vPages = exeMemory->vPages;
	for (uint32_t i = 0u; i < vPages.size; ++i) {
		ketl_executable_memory_page page = vPages.pData[i];
		ketl_deallocate_exe_memory(page.pPage, page.pageSize);
	}

	ketl_executable_memory_page_vector_destroy(&vPages);
}

uint8_t* ketl_executable_memory_allocate(ketl_executable_memory* exeMemory, const uint8_t* opcodes, uint64_t length) {
	uint32_t currentPageIndex = exeMemory->vPages.size;
	uint32_t currentOffset = exeMemory->currentOffset;
	ketl_executable_memory_page currentPage = exeMemory->vPages.pData[currentPageIndex];
	if (currentPage.pPage == NULL || currentOffset + length > currentPage.pageSize) {
		uint32_t pageSize = ketl_get_static_page_size();
		uint32_t requestedPageCount = (uint32_t)((length + (pageSize - 1)) >> ketl_get_static_page_size_log());

		currentPage.pageSize = pageSize * requestedPageCount;
		currentPage.pPage = ketl_allocate_exe_memory(currentPage.pPage, currentPage.pageSize);
		
		ketl_executable_memory_page_vector_push_back_ref(&exeMemory->vPages, &currentPage);
		currentOffset = 0;
	} else {
		ketl_unprotect_exe_memory(currentPage.pPage, currentPage.pageSize);
	}

	uint32_t requestedMemory = (length + 15) & (-16);
	uint8_t* resultMemory = currentPage.pPage + currentOffset;

	memcpy(resultMemory, opcodes, length);
	exeMemory->currentOffset = currentOffset + requestedMemory;

	ketl_protect_exe_memory(currentPage.pPage, currentPage.pageSize);

	return resultMemory;
}
