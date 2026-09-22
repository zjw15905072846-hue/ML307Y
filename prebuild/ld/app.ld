/* Copyright © 2022  China Mobile IOT. 
 * All rights reserved.
 *
 */
 
INCLUDE app_remain_info.ld

OUTPUT_FORMAT("elf64-littleriscv")
OUTPUT_ARCH(riscv:rv64)

MEMORY
{
  FLASH  (xr)   : ORIGIN = 0x60171000 , LENGTH = 0x00073000    /* QSPI-FLASH*/
  PSRAM (xrw)   : ORIGIN = 0x30192000 , LENGTH = 0x000A8D00    /* External PSRAM */
}

/*
 * Default heap definitions.
 * The heap start immediately after the last statically allocated
 * .sbss/.noinit section, and extends up to the main stack limit.
 */
PROVIDE ( _Heap_Begin = _end_noinit ) ;
PROVIDE ( _Heap_Limit = __stack - __Main_Stack_Size ) ;

/*
 * The limit of PSRAM region.
 */
__PSRAM_Limit = ORIGIN(PSRAM) + LENGTH(PSRAM);

ENTRY(cm_opencpu_entry)

#define ALIGNMENT   ALIGN(0x8)

#define IMG_INFO    LONG(APP_REMAIN_BASE_ADDR) \
                    LONG(APP_REMAIN_LENGTH) \
                    LONG(0x5A5AA5A5) \
                    LONG(0x5A5AA5A5)

#define SECTION_INFO(type, section) LONG(type) \
                                    LONG(__##section##_lma_start - __flash_start) \
                                    LONG(__##section##_lma_start) \
                                    LONG(__##section##_vma_start) \
                                    LONG(__##section##_vma_end - __##section##_vma_start)

#define SECTION_TEXT    .text .text.*
#define SECTION_PSRAMTEXT .psramtext
#define SECTION_RODATA  .rdata .rodata .rodata.*
#define SECTION_DATA    .data .data.*
#define SECTION_BSS     .bss .bss.* COMMON .scommon .sbss .sbss.* .sbss2 .sbss2.*

/* Sections Definitions */

SECTIONS
{
    /*
     * flash sections
     */

    . = ORIGIN(FLASH);
    __flash_start = .;

    /*头部信息，lma=vma*/
    .image_header : {
        FILL(0xFF)
        BYTE(0xFF)
        . = ALIGN(0x1000) ;     /* used for storing compressed info */
        LONG(APP_REMAIN_BASE_ADDR)
        LONG(APP_REMAIN_LENGTH)
        LONG(0x5A5AA5A5)
        LONG(0x5A5AA5A5)

        LONG(0)
        LONG(__flash_text_lma_start - __flash_start)
        LONG(__flash_text_lma_start)
        LONG(__flash_text_vma_start)
        LONG(__flash_text_vma_end - __flash_text_vma_start)

        LONG(1)
        LONG(__flash_rodata_lma_start - __flash_start)
        LONG(__flash_rodata_lma_start)
        LONG(__flash_rodata_vma_start)
        LONG(__flash_rodata_vma_end - __flash_rodata_vma_start)

        LONG(2)
        LONG(__psram_text_lma_start - __flash_start)
        LONG(__psram_text_lma_start)
        LONG(__psram_text_vma_start)
        LONG(__psram_text_vma_end - __psram_text_vma_start)

        LONG(3)
        LONG(__psram_rodata_lma_start - __flash_start)
        LONG(__psram_rodata_lma_start)
        LONG(__psram_rodata_vma_start)
        LONG(__psram_rodata_vma_end - __psram_rodata_vma_start)

        LONG(4)
        LONG(__psram_data_lma_start - __flash_start)
        LONG(__psram_data_lma_start)
        LONG(__psram_data_vma_start)
        LONG(__psram_data_vma_end - __psram_data_vma_start)

        LONG(5)
        LONG(__psram_bss_lma_start - __flash_start)
        LONG(__psram_bss_lma_start)
        LONG(__psram_bss_vma_start)
        LONG(__psram_bss_vma_end - __psram_bss_vma_start)
    } > FLASH

    .flash.text : ALIGN(8)
    {
        __flash_text_vma_start = .;

        *(.text.cm_opencpu_entry)
        EXCLUDE_FILE(*.psram.o *libm_nano.a: *libc_nano.a: *libgcc.a:) *(.text .text.*)

        /* for cache line aligned, the code and data are no longer in the same cache line. */
        . = ALIGN(0x20) ;

        __flash_text_vma_end = .;

        __flash_text_lma_start = LOADADDR(.flash.text);
    } >FLASH

    .flash.rodata : ALIGN(8)
    {
        __flash_rodata_vma_start = .;

	EXCLUDE_FILE(*.psram.o *libm_nano.a: *libc_nano.a: *libgcc.a:) *(.constdata .constdata.* .rodata .rodata.*)
        *(.constdata .constdata.* .rodata .rodata.*)

        . = ALIGN(0x8) ;

        __flash_rodata_vma_end = .;

        __flash_rodata_lma_start = LOADADDR(.flash.rodata);
    } >FLASH

    /*
     * psram sections
     */

    .psram.text : ALIGN(8)
    {
        __psram_text_vma_start = . ;

        LONG(_end_noinit)
        LONG(__PSRAM_Limit)

        *.psram.o(.text .text.*)
        *libc_nano.a:(.text .text.*)
        *libgcc.a:(.text .text.*)
        *libm_nano.a:(.text .text.*)
        *(.psramtext)

        /* for cache line aligned, the code and data are no longer in the same cache line. */
        . = ALIGN(0x20);

        __psram_text_vma_end = .;

        __psram_text_lma_start = LOADADDR(.psram.text);
    } >PSRAM

    .psram.rodata : ALIGN(8)
    {
        __psram_rodata_vma_start = . ;

	*.psram.o(.rodata .rodata.* .srodata .srodata.*)

        . = ALIGN(0x8) ;

        __psram_rodata_vma_end = . ;

        __psram_rodata_lma_start = LOADADDR(.psram.rodata);
    } >PSRAM

    _psram_sidata = LOADADDR(.psram.data);

    .psram.data : ALIGN(8)
    {
        FILL(0xFF)
        __psram_data_vma_start = . ;

        *(.data .data.* .sdata .sdata.*)

        . = ALIGN(0x8);
        __psram_data_vma_end = . ;

        __psram_data_lma_start = LOADADDR(.psram.data);
    } >PSRAM

    .psram.bss (NOLOAD) : ALIGN(8)
    {
        __psram_bss_vma_start = . ;

        *(.bss .bss.* .sbss .sbss.*)
        . = ALIGN(8);

        __psram_bss_vma_end = . ;

        . = ALIGN(8);

        __psram_bss_lma_start = LOADADDR(.psram.bss);
    } >PSRAM

    .noinit (NOLOAD) : ALIGN(8)
    {
        _noinit = . ;

        *(.noinit .noinit.*)

        . = ALIGN(8) ;

        _end_noinit = . ;
    } > PSRAM

    /* the start and end address of psram free region. */
    _PSRAM_Free_Begin = _end_noinit ;
    _PSRAM_Free_Limit = __PSRAM_Limit;

    /* Mandatory to be word aligned, _sbrk assumes this */
    PROVIDE ( end = _end_noinit ); /* was _ebss */
    PROVIDE ( _end = _end_noinit );
    PROVIDE ( __end = _end_noinit );
    PROVIDE ( __end__ = _end_noinit );

    /* After that there are only debugging sections. */

    /* This can remove the debugging information from the standard libraries */
    /*
    DISCARD :
    {
     libc.a ( * )
     libm.a ( * )
     libgcc.a ( * )
     }
     */

    /* Stabs debugging sections.  */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }
    .comment       0 : { *(.comment) }
    /*
     * DWARF debug sections.
     * Symbols in the DWARF debugging sections are relative to the beginning
     * of the section so we begin them at 0.
     */
    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /* SGI/MIPS DWARF 2 extensions */
    .debug_weaknames 0 : { *(.debug_weaknames) }
    .debug_funcnames 0 : { *(.debug_funcnames) }
    .debug_typenames 0 : { *(.debug_typenames) }
    .debug_varnames  0 : { *(.debug_varnames) }
    /* DWARF 3 */
    .debug_ranges    0 : { *(.debug_ranges) }
    .debug_pubtypes  0 : { *(.debug_pubtypes) }
    .debug_macro     0 : { *(.debug_macro) }
    /* DWARF 4 */
    .debug_types     0 : { *(.debug_types) }
    .debug_addr      0 : { *(.debug_addr) }
    /* DWARF 5 */
    .debug_gnu_pubtypes 0 : { *(.debug_gnu_pubtypes) }
    .debug_gnu_pubnames 0 : { *(.debug_gnu_pubnames) }
    .debug_line_str     0 : { *(.debug_line_str) }
    .debug_str_offsets  0 : { *(.debug_str_offsets) }
    .debug_loclists     0 : { *(.debug_loclists) }
    .debug_rnglists     0 : { *(.debug_rnglists) }
    .debug_names        0 : { *(.debug_names) }
    /* Others */
    .note.GNU-stack         0 : { *(.note.GNU-stack) }
    .note.ABI-tag           0 : { *(.note.ABI-tag) }
    .gnu.build.attributes   0 : { *(.gnu.build.attributes) }
    .riscv.attributes       0 : { *(.riscv.attributes) }
    .rela                   0 : { *(.rela .rela.*) }
}
