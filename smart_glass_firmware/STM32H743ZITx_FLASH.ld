ENTRY(Reset_Handler)

_Min_Heap_Size = 0x40000;
_Min_Stack_Size = 0x1000;

MEMORY
{
    FLASH  (rx) : ORIGIN = 0x08000000, LENGTH = 2048K
    DTCM   (rw) : ORIGIN = 0x20000000, LENGTH = 128K
    AXI_SRAM (rw) : ORIGIN = 0x24000000, LENGTH = 512K
    SRAM1  (rw) : ORIGIN = 0x30000000, LENGTH = 128K
    SRAM2  (rw) : ORIGIN = 0x30020000, LENGTH = 128K
    SRAM3  (rw) : ORIGIN = 0x30040000, LENGTH = 32K
}

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH

    .text : {
        . = ALIGN(4);
        *(.text) *(.text.*)
        *(.glue_7) *(.glue_7t)
        *(.gnu.linkonce.t.*)
        *(.rodata) *(.rodata.*)
        *(.gnu.linkonce.r.*)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    _sidata = LOADADDR(.data);

    .data : {
        . = ALIGN(4);
        _sdata = .;
        *(.data) *(.data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(4);
        _edata = .;
    } > AXI_SRAM AT > FLASH

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss) *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > AXI_SRAM

    _estack = ORIGIN(AXI_SRAM) + LENGTH(AXI_SRAM);
    _sstack = _estack - _Min_Stack_Size;

    PROVIDE ( end = ORIGIN(DTCM) + LENGTH(DTCM) );
    PROVIDE ( _end = end );

    .ARM.exidx : { *(.ARM.exidx*) } > FLASH
    /DISCARD/ : { *(.note.gnu.build-id) *(.comment) }
}
