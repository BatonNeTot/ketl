#include "dynamic_library.h"

#include <stdio.h>
#include <string.h>

#if ANN_OS_WINDOWS

#define setpword(p_buffer, offset, value) { *(uint16_t*)(&(p_buffer)[(offset)]) = (value); }
#define setdword(p_buffer, offset, value) { *(uint32_t*)(&(p_buffer)[(offset)]) = (value); }
#define setqword(p_buffer, offset, value) { *(uint64_t*)(&(p_buffer)[(offset)]) = (value); }
#define setccstr(p_buffer, offset, p_str) { strcpy((char*)&(p_buffer)[(offset)], (p_str)); }


typedef struct {
    uint64_t address_table;
    uint64_t lookup_table;
    uint64_t name_table;
    uint64_t name;
} import_table_rva_info;

void ketl_dynamic_library_flush_function(export_header* p_export, import_header* p_import, const uint64_t import_count) {
    uint8_t buffer[32768] = {0};

/////////////////////////////

    uint64_t export_name_indirection[256];

    for (uint64_t i = 0; i < p_export->count; ++i) {
        export_name_indirection[i] = i;
    }

    for (uint64_t i = p_export->count; i > 0;) {
        --i;
        for (uint64_t j = 0; j < i; ++j) {
            if (strcmp(p_export->p_infos[export_name_indirection[j]].p_name, p_export->p_infos[export_name_indirection[j + 1]].p_name) > 0) {
                uint64_t tmp = export_name_indirection[j];
                export_name_indirection[j] = export_name_indirection[j + 1];
                export_name_indirection[j + 1] = tmp; 
            }
        }
    }

/////////////////////////////

    uint64_t code_size = 0;
    for (uint64_t i = 0u; i < p_export->count; ++i) {
        code_size = ANN_ALIGN_FORWARD(code_size, 16);
        code_size += p_export->p_infos[i].opcodes_size;
    }

////////////////////////////

    import_table_rva_info a_import_table_rvas[16] = {0};
    uint64_t import_address_table_rva = 0;
    uint64_t import_address_table_size = 0;

    uint64_t export_table_rva = 0;
    uint64_t export_table_size = 0;

    uint64_t export_address_table_rva = 0;
    uint64_t export_name_pointer_table_rva = 0;
    uint64_t export_ordinal_table_rva = 0;
    uint64_t export_name_table_rva = 0;

    uint64_t import_table_rva = 0;
    uint64_t import_table_size = 0;

    uint64_t rdata_size = 0;

    ////////

    import_address_table_rva = rdata_size;

    for (uint64_t i = 0u; i < import_count; ++i) {
        a_import_table_rvas[i].address_table = rdata_size; 
        for (uint64_t j = 0u; j < p_import[i].count; ++j) {
            rdata_size += 8;
        }
        rdata_size += 8;
    }

    import_address_table_size = rdata_size - import_address_table_rva;

    /////////

    rdata_size = ANN_ALIGN_FORWARD(rdata_size, 4);
    export_table_rva = rdata_size;

    rdata_size += 40; // Export Directory Table

    export_address_table_rva = rdata_size;
    rdata_size += 4 * p_export->count;

    export_name_pointer_table_rva = rdata_size;
    rdata_size += 4 * p_export->count;

    export_ordinal_table_rva = rdata_size;
    rdata_size += 2 * p_export->count;

    export_name_table_rva = rdata_size;
    rdata_size += strlen(p_export->p_filename) + 1;
    for (uint64_t i = 0u; i < p_export->count; ++i) {
        rdata_size += strlen(p_export->p_infos[i].p_name) + 1;
    }

    export_table_size = rdata_size - export_table_rva;

    ///////////
    
    if (import_count > 0) {
        rdata_size = ANN_ALIGN_FORWARD(rdata_size, 4);
        import_table_rva = rdata_size;

        rdata_size += 20 * (import_count + 1);

        rdata_size = ANN_ALIGN_FORWARD(rdata_size, 8);
        for (uint64_t i = 0u; i < import_count; ++i) {
            a_import_table_rvas[i].lookup_table = rdata_size; 
            for (uint64_t j = 0u; j < p_import[i].count; ++j) {
                rdata_size += 8;
            }
            rdata_size += 8;
        }

        for (uint64_t i = 0u; i < import_count; ++i) {
            rdata_size = ANN_ALIGN_FORWARD(rdata_size, 2);
            a_import_table_rvas[i].name_table = rdata_size; 
            
            for (uint64_t j = 0u; j < p_import[i].count; ++j) {
                rdata_size = ANN_ALIGN_FORWARD(rdata_size, 2);
                rdata_size += 2 + strlen(p_import[i].p_imports[j].p_name) + 1;
            }

            rdata_size = ANN_ALIGN_FORWARD(rdata_size, 2);
        }

        for (uint64_t i = 0u; i < import_count; ++i) {
            a_import_table_rvas[i].name = rdata_size; 
            rdata_size += strlen(p_import[i].p_filename) + 2;
        }

        import_table_size = rdata_size - import_table_rva;
    }

////////////////////////////

#define PE_OFFSET 68 // (sizeof(dos_header_stub) + 4)
#define OPT_OFFSET (PE_OFFSET + 20)
#define OPT_NUMBER_OF_RVA 14
#define OPT_SIZE (112 + OPT_NUMBER_OF_RVA * 8)

#define SECTION_TEXT_OFFSET (OPT_OFFSET + OPT_SIZE)
#define SECTION_RDATA_OFFSET (SECTION_TEXT_OFFSET + 40)
#define SECTION_END_OFFSET (SECTION_RDATA_OFFSET + 40)

#define SECTION_ALIGNMENT 0x1000

    uint64_t code_section_offset = SECTION_ALIGNMENT;
    uint64_t rdata_section_offset = ANN_ALIGN_FORWARD(code_section_offset + code_size, SECTION_ALIGNMENT);
    uint64_t end_section_offset = ANN_ALIGN_FORWARD(rdata_section_offset + rdata_size, SECTION_ALIGNMENT);

#define FILE_ALIGNMENT 0x200

    uint64_t code_file_offset = ANN_ALIGN_FORWARD(SECTION_END_OFFSET, FILE_ALIGNMENT);
    uint64_t rdata_file_offset = ANN_ALIGN_FORWARD(code_file_offset + code_size, FILE_ALIGNMENT);
    uint64_t end_file_offset = ANN_ALIGN_FORWARD(rdata_file_offset + rdata_size, FILE_ALIGNMENT);

////////////////////////////

    // 1. DOS HEADER, 64 bytes
    setccstr(buffer, 0, "MZ"); // DOS header signature is 'MZ'
    setdword(buffer, 60, 64); // DOS e_lfanew field gives the file offset to the PE header

    // 2. PE HEADER, at offset DOS.e_lfanew, 24 bytes
    setccstr(buffer, PE_OFFSET - 4, "PE"); // PE header signature is 'PE\0\0'
    setpword(buffer, PE_OFFSET + 0, 0x8664); // PE.Machine = IMAGE_FILE_MACHINE_AMD64
    setpword(buffer, PE_OFFSET + 2, 2); // PE.NumberOfSections
    //setdword(buffer, PE_OFFSET + 4, 0x6910F4B6); // Time data
    setpword(buffer, PE_OFFSET + 16, OPT_SIZE); // PE.SizeOfOptionalHeader = offset between the optional header and the section table
    setpword(buffer, PE_OFFSET + 18, 0x2022); // PE.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL  | IMAGE_FILE_LARGE_ADDRESS_AWARE

    // 3. OPTIONAL HEADER, follows PE header
    setpword(buffer, OPT_OFFSET + 0, 0x20B); // Opt.Magic = Optional header signature PE32+
    //setpbyte(buffer, OPT_OFFSET + 2, 14); // Opt.MajorLinkerVersion 
    //setpbyte(buffer, OPT_OFFSET + 3, 29); // Opt.MinorLinkerVersion 
    setdword(buffer, OPT_OFFSET + 4, rdata_file_offset - code_file_offset); // Opt.SizeOfCode
    setdword(buffer, OPT_OFFSET + 8, end_file_offset - rdata_file_offset); // Opt.SizeOfInitializedData 
    setdword(buffer, OPT_OFFSET + 16, 0); // Opt.AddressOfEntryPoint = RVA where code execution should begin
    setdword(buffer, OPT_OFFSET + 20, code_section_offset); // Opt.BaseOfCode = The address that is relative to the image base of the beginning-of-code section when it is loaded into memory.
    setqword(buffer, OPT_OFFSET + 24, 0x180000000); // Opt.ImageBase = base address at which to load the program, 0x400000 is standard
    setdword(buffer, OPT_OFFSET + 32, SECTION_ALIGNMENT); // Opt.SectionAlignment = alignment of section in memory at run-time, 4096 is standard
    setdword(buffer, OPT_OFFSET + 36, FILE_ALIGNMENT); // Opt.FileAlignment = alignment of sections in file, 512 is standard
    //setpword(buffer, OPT_OFFSET + 40, 6); // Opt.MajorOperatingSystemVersion  = The major version number of the required operating system. 
    setpword(buffer, OPT_OFFSET + 48, 4); // Opt.MajorSubsystemVersion = minimum OS version required to run this program
    setdword(buffer, OPT_OFFSET + 56, end_section_offset); // Opt.SizeOfImage = total run-time memory size of all sections and headers
    setdword(buffer, OPT_OFFSET + 60, code_file_offset); // Opt.SizeOfHeaders = total file size of header info before the first section
    setpword(buffer, OPT_OFFSET + 68, 3); // Opt.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI, command-line program
    //setpword(buffer, OPT_OFFSET + 68, 2); // Opt.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI
    setpword(buffer, OPT_OFFSET + 70, 0x160); // Opt.DllCharacteristics = IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA | IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE | IMAGE_DLLCHARACTERISTICS_NX_COMPAT
    setqword(buffer, OPT_OFFSET + 72, 0x100000); // Opt.SizeOfStackReserve = The size of the stack to reserve. Only SizeOfStackCommit is committed; the rest is made available one page at a time until the reserve size is reached. 
    setqword(buffer, OPT_OFFSET + 80, 0x1000); // Opt.SizeOfStackCommit  = The size of the stack to commit. 
    setqword(buffer, OPT_OFFSET + 88, 0x100000); // Opt.SizeOfHeapReserve = The size of the heap to reserve. Only SizeOfHeapCommit is committed; the rest is made available one page at a time until the reserve size is reached. 
    setqword(buffer, OPT_OFFSET + 96, 0x1000); // Opt.SizeOfHeapCommit  = The size of the heap to commit. 
    setdword(buffer, OPT_OFFSET + 108, OPT_NUMBER_OF_RVA); // Opt.NumberOfRvaAndSizes = number of data directories following
    
    // 4. DATA DIRECTORIES, follows optional header, 8 bytes per directory 
    // Export Table
    setdword(buffer, OPT_OFFSET + 112, rdata_section_offset + export_table_rva); // DataDir.VirtualAddress
    setdword(buffer, OPT_OFFSET + 116, export_table_size); // DataDir.Size
    if (import_count > 0) {
        // Import Table
        setdword(buffer, OPT_OFFSET + 120, rdata_section_offset + import_table_rva); // DataDir.VirtualAddress
        setdword(buffer, OPT_OFFSET + 124, import_table_size); // DataDir.Size
        // Import Address Table
        setdword(buffer, OPT_OFFSET + 208, rdata_section_offset + import_address_table_rva); // DataDir.VirtualAddress
        setdword(buffer, OPT_OFFSET + 212, import_address_table_size); // DataDir.Size
    }
    

    // 5. SECTION TABLE, follows data directories, 40 bytes
#define SECTION_TEXT_OFFSET (OPT_OFFSET + OPT_SIZE)
    setccstr(buffer, SECTION_TEXT_OFFSET + 0, ".text"); // name of 1st section
    setdword(buffer, SECTION_TEXT_OFFSET + 8, code_size); // sectHdr.VirtualSize = size of the section in memory at run-time
    setdword(buffer, SECTION_TEXT_OFFSET + 12, code_section_offset); // sectHdr.VirtualAddress = RVA for the section
    setdword(buffer, SECTION_TEXT_OFFSET + 16, rdata_file_offset - code_file_offset); // sectHdr.SizeOfRawData = size of the section data in the file
    setdword(buffer, SECTION_TEXT_OFFSET + 20, code_file_offset); // sectHdr.PointerToRawData = file offset of this section's data
    setdword(buffer, SECTION_TEXT_OFFSET + 36, 0x60000020); // sectHdr.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE

#define SECTION_RDATA_OFFSET (SECTION_TEXT_OFFSET + 40)
    setccstr(buffer, SECTION_RDATA_OFFSET + 0, ".rdata"); // name of 1st section
    setdword(buffer, SECTION_RDATA_OFFSET + 8, rdata_size); // sectHdr.VirtualSize = size of the section in memory at run-time
    setdword(buffer, SECTION_RDATA_OFFSET + 12, rdata_section_offset); // sectHdr.VirtualAddress = RVA for the section
    setdword(buffer, SECTION_RDATA_OFFSET + 16, end_file_offset - rdata_file_offset); // sectHdr.SizeOfRawData = size of the section data in the file
    setdword(buffer, SECTION_RDATA_OFFSET + 20, rdata_file_offset); // sectHdr.PointerToRawData = file offset of this section's data
    setdword(buffer, SECTION_RDATA_OFFSET + 36, 0x40000040); // sectHdr.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA

    // 6. .TEXT SECTION, at sectHdr.PointerToRawData (aligned to Opt.FileAlignment)
    {
        uint64_t code_offset = 0;
        for (uint64_t i = 0u; i < p_export->count; ++i) {
            code_offset = ANN_ALIGN_FORWARD(code_offset, 16);
            memcpy(buffer + 0x200 + code_offset, p_export->p_infos[i].p_opcodes, p_export->p_infos[i].opcodes_size);
            code_offset += p_export->p_infos[i].opcodes_size;
        }
    }

    // 7. .RDATA SECTION, at sectHdr.PointerToRawData (aligned to Opt.FileAlignment)

    ///////////////////

    setdword(buffer, rdata_file_offset + export_table_rva + 4, 0xFFFFFFFF); // Time/Date Stamp 
    setdword(buffer, rdata_file_offset + export_table_rva + 12, rdata_section_offset + export_name_table_rva); // Name RVA. The address of the ASCII string that contains the name of the DLL. This address is relative to the image base. 
    setdword(buffer, rdata_file_offset + export_table_rva + 16, 1); // Ordinal Base
    setdword(buffer, rdata_file_offset + export_table_rva + 20, p_export->count); // Address Table Entries
    setdword(buffer, rdata_file_offset + export_table_rva + 24, p_export->count); // Number of Name Pointers 
    setdword(buffer, rdata_file_offset + export_table_rva + 28, rdata_section_offset + export_address_table_rva); // Export Address Table RVA 
    setdword(buffer, rdata_file_offset + export_table_rva + 32, rdata_section_offset + export_name_pointer_table_rva); // Name Pointer RVA 
    setdword(buffer, rdata_file_offset + export_table_rva + 36, rdata_section_offset + export_ordinal_table_rva); // Ordinal Table RVA 
    
    {
        uint64_t code_section_accum = code_section_offset;
        for (uint64_t i = 0u; i < p_export->count; ++i) {
            code_section_accum = ANN_ALIGN_FORWARD(code_section_accum, 16);
            setdword(buffer, rdata_file_offset + export_address_table_rva + 4 * i, code_section_accum); // Export RVA Array 
            code_section_accum += p_export->p_infos[i].opcodes_size;
        }
    }

    {
        uint64_t export_name_table_offset = rdata_section_offset + export_name_table_rva;
        export_name_table_offset += strlen(p_export->p_filename) + 1;
        for (uint64_t i = 0u; i < p_export->count; ++i) {
            setdword(buffer, rdata_file_offset + export_name_pointer_table_rva + 4 * i, export_name_table_offset);
            export_name_table_offset += strlen(p_export->p_infos[export_name_indirection[i]].p_name) + 1;
        }
    }
    
    for (uint64_t i = 0u; i < p_export->count; ++i) {
        setpword(buffer, rdata_file_offset + export_ordinal_table_rva + 2 * i, export_name_indirection[i]); // Ordinal Table RVA 
    }
    
    {
        uint64_t export_name_table_offset = export_name_table_rva;
        setccstr(buffer, rdata_file_offset + export_name_table_offset, p_export->p_filename);
        export_name_table_offset += strlen(p_export->p_filename) + 1;
        for (uint64_t i = 0u; i < p_export->count; ++i) {
            setccstr(buffer, rdata_file_offset + export_name_table_offset, p_export->p_infos[export_name_indirection[i]].p_name);
            export_name_table_offset += strlen(p_export->p_infos[export_name_indirection[i]].p_name) + 1;
        }
    }

    ////////////////

    for (uint64_t i = 0u; i < import_count; ++i) {
        setdword(buffer, rdata_file_offset + import_table_rva + 20 * i + 0, rdata_section_offset + a_import_table_rvas[i].lookup_table); // Lookup Table RVA
        setdword(buffer, rdata_file_offset + import_table_rva + 20 * i + 4, 0); // Time/Date Stamp 
        setdword(buffer, rdata_file_offset + import_table_rva + 20 * i + 8, 0); // Forwarder Chain index, not in use 
        setdword(buffer, rdata_file_offset + import_table_rva + 20 * i + 12, rdata_section_offset + a_import_table_rvas[i].name); // Name RVA. The address of the ASCII string that contains the name of the DLL. This address is relative to the image base. 
        setdword(buffer, rdata_file_offset + import_table_rva + 20 * i + 16, rdata_section_offset + a_import_table_rvas[i].address_table); // address Table Rva
    }

    {
        for (uint64_t i = 0u; i < import_count; ++i) {
            uint64_t import_name_table_offset = a_import_table_rvas[i].name_table;
            setccstr(buffer, rdata_file_offset + a_import_table_rvas[i].name, p_import[i].p_filename);
            
            for (uint64_t j = 0u; j < p_import[i].count; ++j) {
                import_name_table_offset = ANN_ALIGN_FORWARD(import_name_table_offset, 2);
                setqword(buffer, rdata_file_offset + a_import_table_rvas[i].address_table + 8 * j, rdata_section_offset + import_name_table_offset); // Lookup Entry
                setqword(buffer, rdata_file_offset + a_import_table_rvas[i].lookup_table + 8 * j, rdata_section_offset + import_name_table_offset); // Lookup Entry
                setpword(buffer, rdata_file_offset + import_name_table_offset, p_import[i].p_imports[j].index);
                setccstr(buffer, rdata_file_offset + import_name_table_offset + 2, p_import[i].p_imports[j].p_name);
                import_name_table_offset += 2 + strlen(p_import[i].p_imports[j].p_name) + 1;
            }
        }
    }

    ////////////////

    FILE* p_file = fopen(p_export->p_filename, "wb");
    assert(end_file_offset <= ANN_ARRAY_SIZE(buffer));
    fwrite(buffer, 1, end_file_offset, p_file); // size of PE
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
