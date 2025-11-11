#include "dynamic_library.h"

#include <stdio.h>
#include <string.h>

#if ANN_OS_WINDOWS

inline static void setpword(uint8_t* pBuf, uint32_t off, uint16_t val) { *(uint16_t*)(&pBuf[off]) = val; }
inline static void setdword(uint8_t* pBuf, uint32_t off, uint32_t val) { *(uint32_t*)(&pBuf[off]) = val; }
inline static void setqword(uint8_t* pBuf, uint32_t off, uint64_t val) { *(uint64_t*)(&pBuf[off]) = val; }
inline static void setccstr(uint8_t* pBuf, uint32_t off, const char* val) { strcpy((char*)&pBuf[off], val); }

void ketl_dynamic_library_flush_function(const char* p_filename, const char* p_function_name, uint8_t* opcodes, uint32_t size) {

#define EXPORT_ADDRESS_TABLE_RVA        40
#define EXPORT_NAME_POINTER_TABLE_RVA   (EXPORT_ADDRESS_TABLE_RVA + NUMBER_OF_EXPORTS * 4)
#define EXPORT_ORDINAL_TABLE_RVA        (EXPORT_NAME_POINTER_TABLE_RVA + NUMBER_OF_EXPORTS * 4)
#define EXPORT_NAME_TABLE_RVA           (EXPORT_ORDINAL_TABLE_RVA + NUMBER_OF_EXPORTS * 2)

#define DLL_NAME_RVA                    EXPORT_NAME_TABLE_RVA

#define NUMBER_OF_EXPORTS 1

    uint32_t name_table_size = 0;
    uint32_t filename_size = strlen(p_filename) + 1;
    name_table_size += filename_size;
    name_table_size += strlen(p_function_name) + 1;

    uint32_t export_table_size = EXPORT_NAME_TABLE_RVA + name_table_size;

#define FILE_ALIGNMENT 0x200
#define SECTION_BEGIN 0x200

#define CODE_SECTION_BEGIN SECTION_BEGIN

    uint32_t code_section_size = size;
    code_section_size = ANN_ALIGN_FORWARD(code_section_size, FILE_ALIGNMENT);

    uint32_t export_section_begin = CODE_SECTION_BEGIN + code_section_size;

    uint32_t export_section_size = EXPORT_NAME_TABLE_RVA + name_table_size;
    export_section_size = ANN_ALIGN_FORWARD(export_section_size, FILE_ALIGNMENT);

uint8_t buffer[2048] = {0};

    // 1. DOS HEADER, 64 bytes
    setccstr(buffer, 0, "MZ"); // DOS header signature is 'MZ'
    setdword(buffer, 60, 64); // DOS e_lfanew field gives the file offset to the PE header

#define CODE_DATA_RVA 0x1000
#define EXPORT_TABLE_RVA 0x2000

#define SECTION_HEADER_SIZE 40

#define OPT_NUMBER_OF_RVA 8
#define OPT_SIZE (112 + OPT_NUMBER_OF_RVA * 8)

    // 2. PE HEADER, at offset DOS.e_lfanew, 24 bytes
 #define PE_OFFSET 68//(ANN_ARRAY_SIZE(dos_header_stub) + 4)
    setccstr(buffer, PE_OFFSET - 4, "PE"); // PE header signature is 'PE\0\0'
    setpword(buffer, PE_OFFSET + 0, 0x8664); // PE.Machine = IMAGE_FILE_MACHINE_AMD64
    setpword(buffer, PE_OFFSET + 2, 2); // PE.NumberOfSections
    setpword(buffer, PE_OFFSET + 16, OPT_SIZE); // PE.SizeOfOptionalHeader = offset between the optional header and the section table
    setpword(buffer, PE_OFFSET + 18, 0x2022); // PE.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL  | IMAGE_FILE_LARGE_ADDRESS_AWARE

    // 3. OPTIONAL HEADER, follows PE header
#define OPT_OFFSET (PE_OFFSET + 20)
    setpword(buffer, OPT_OFFSET + 0, 0x20B); // Opt.Magic = Optional header signature PE32+
    setdword(buffer, OPT_OFFSET + 4, code_section_size); // Opt.SizeOfCode
    setdword(buffer, OPT_OFFSET + 8, export_section_size); // Opt.SizeOfInitializedData 
    setdword(buffer, OPT_OFFSET + 16, 0); // Opt.AddressOfEntryPoint = RVA where code execution should begin
    setdword(buffer, OPT_OFFSET + 20, CODE_DATA_RVA); // Opt.BaseOfCode = The address that is relative to the image base of the beginning-of-code section when it is loaded into memory.
    setqword(buffer, OPT_OFFSET + 24, 0x180000000); // Opt.ImageBase = base address at which to load the program, 0x400000 is standard
    setdword(buffer, OPT_OFFSET + 32, 0x1000); // Opt.SectionAlignment = alignment of section in memory at run-time, 4096 is standard
    setdword(buffer, OPT_OFFSET + 36, FILE_ALIGNMENT); // Opt.FileAlignment = alignment of sections in file, 512 is standard
    setpword(buffer, OPT_OFFSET + 48, 4); // Opt.MajorSubsystemVersion = minimum OS version required to run this program
    setdword(buffer, OPT_OFFSET + 56, 0x3000); // Opt.SizeOfImage = total run-time memory size of all sections and headers
    setdword(buffer, OPT_OFFSET + 60, code_section_size + export_section_size); // Opt.SizeOfHeaders = total file size of header info before the first section
    setpword(buffer, OPT_OFFSET + 68, 3); // Opt.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI, command-line program
    setpword(buffer, OPT_OFFSET + 70, 0x160); // Opt.DllCharacteristics = IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA | IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE | IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    setqword(buffer, OPT_OFFSET + 72, 0x100000); // Opt.SizeOfStackReserve = The size of the stack to reserve. Only SizeOfStackCommit is committed; the rest is made available one page at a time until the reserve size is reached. 
    setqword(buffer, OPT_OFFSET + 80, 0x1000); // Opt.SizeOfStackCommit  = The size of the stack to commit. 
    setqword(buffer, OPT_OFFSET + 88, 0x100000); // Opt.SizeOfHeapReserve = The size of the heap to reserve. Only SizeOfHeapCommit is committed; the rest is made available one page at a time until the reserve size is reached. 
    setqword(buffer, OPT_OFFSET + 96, 0x1000); // Opt.SizeOfHeapCommit  = The size of the heap to commit. 
    setdword(buffer, OPT_OFFSET + 108, OPT_NUMBER_OF_RVA); // Opt.NumberOfRvaAndSizes = number of data directories following
    
    // 4. DATA DIRECTORIES, follows optional header, 8 bytes per directory 
    setdword(buffer, OPT_OFFSET + 112, EXPORT_TABLE_RVA); // DataDir.VirtualAddress
    setdword(buffer, OPT_OFFSET + 116, export_table_size); // DataDir.Size

    // 5. SECTION TABLE, follows data directories, 40 bytes
#define SECTION_TEXT_OFFSET (OPT_OFFSET + OPT_SIZE)
    setccstr(buffer, SECTION_TEXT_OFFSET + 0, ".text"); // name of 1st section
    setdword(buffer, SECTION_TEXT_OFFSET + 8, size); // sectHdr.VirtualSize = size of the section in memory at run-time
    setdword(buffer, SECTION_TEXT_OFFSET + 12, CODE_DATA_RVA); // sectHdr.VirtualAddress = RVA for the section
    setdword(buffer, SECTION_TEXT_OFFSET + 16, code_section_size); // sectHdr.SizeOfRawData = size of the section data in the file
    setdword(buffer, SECTION_TEXT_OFFSET + 20, CODE_SECTION_BEGIN); // sectHdr.PointerToRawData = file offset of this section's data
    setdword(buffer, SECTION_TEXT_OFFSET + 36, 0x60000020); // sectHdr.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE

#define SECTION_RDATA_OFFSET (SECTION_TEXT_OFFSET + SECTION_HEADER_SIZE)
    setccstr(buffer, SECTION_RDATA_OFFSET + 0, ".rdata"); // name of 1st section
    setdword(buffer, SECTION_RDATA_OFFSET + 8, export_table_size); // sectHdr.VirtualSize = size of the section in memory at run-time
    setdword(buffer, SECTION_RDATA_OFFSET + 12, EXPORT_TABLE_RVA); // sectHdr.VirtualAddress = RVA for the section
    setdword(buffer, SECTION_RDATA_OFFSET + 16, export_section_size); // sectHdr.SizeOfRawData = size of the section data in the file
    setdword(buffer, SECTION_RDATA_OFFSET + 20, export_section_begin); // sectHdr.PointerToRawData = file offset of this section's data
    setdword(buffer, SECTION_RDATA_OFFSET + 36, 0x40000040); // sectHdr.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA

    // 6. .TEXT SECTION, at sectHdr.PointerToRawData (aligned to Opt.FileAlignment)
    memcpy(buffer + CODE_SECTION_BEGIN, opcodes, size);

#define DATA_RDATA_OFFSET export_section_begin
    setdword(buffer, DATA_RDATA_OFFSET + 4, 0xFFFFFFFF); // Time/Date Stamp 
    setdword(buffer, DATA_RDATA_OFFSET + 12, EXPORT_TABLE_RVA + DLL_NAME_RVA); // Name RVA. The address of the ASCII string that contains the name of the DLL. This address is relative to the image base. 
    setdword(buffer, DATA_RDATA_OFFSET + 16, 1); // Ordinal Base
    setdword(buffer, DATA_RDATA_OFFSET + 20, NUMBER_OF_EXPORTS); // Address Table Entries
    setdword(buffer, DATA_RDATA_OFFSET + 24, NUMBER_OF_EXPORTS); // Number of Name Pointers 
    setdword(buffer, DATA_RDATA_OFFSET + 28, EXPORT_TABLE_RVA + EXPORT_ADDRESS_TABLE_RVA); // Export Address Table RVA 
    setdword(buffer, DATA_RDATA_OFFSET + 32, EXPORT_TABLE_RVA + EXPORT_NAME_POINTER_TABLE_RVA); // Name Pointer RVA 
    setdword(buffer, DATA_RDATA_OFFSET + 36, EXPORT_TABLE_RVA + EXPORT_ORDINAL_TABLE_RVA); // Ordinal Table RVA 
    
    setdword(buffer, DATA_RDATA_OFFSET + EXPORT_ADDRESS_TABLE_RVA + 0, CODE_DATA_RVA); // Export RVA Array 
    
    setdword(buffer, DATA_RDATA_OFFSET + EXPORT_NAME_POINTER_TABLE_RVA + 0, EXPORT_TABLE_RVA + EXPORT_NAME_TABLE_RVA + 9); // Name Pointer RVA Array

    setpword(buffer, DATA_RDATA_OFFSET + EXPORT_ORDINAL_TABLE_RVA + 0, 0); // Ordinal Table RVA 
    
    setccstr(buffer, DATA_RDATA_OFFSET + EXPORT_NAME_TABLE_RVA + 0, p_filename);
    setccstr(buffer, DATA_RDATA_OFFSET + EXPORT_NAME_TABLE_RVA + filename_size, p_function_name);

    FILE* p_file = fopen(p_filename, "wb");
    fwrite(buffer, 1, DATA_RDATA_OFFSET + export_section_size, p_file); // size of PE
    fclose(p_file);
}

#include "Windows.h"

ketl_dynamic_library_func_t ketl_dynamic_library_load_function(const char* p_filename, const char* p_function_name) {
    HINSTANCE hGetProcIDDLL = LoadLibrary(p_filename);

    if (!hGetProcIDDLL) {
        return NULL;
    }

    return (ketl_dynamic_library_func_t)GetProcAddress(hGetProcIDDLL, p_function_name);
}
#endif
