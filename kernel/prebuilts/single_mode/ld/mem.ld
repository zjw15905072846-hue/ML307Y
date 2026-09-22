/*
 * Memory Spaces Definitions.
 *
 * Need modifying for a specific board. 
 *   FLASH.ORIGIN: starting address of flash
 *   FLASH.LENGTH: length of flash
 *   RAM.ORIGIN: starting address of RAM bank 0
 *   RAM.LENGTH: length of RAM bank 0
 *
 * The values below can be addressed in further linker scripts
 * using functions like 'ORIGIN(RAM)' or 'LENGTH(RAM)'.
 */

MEMORY
{
  FLASH  (xr)   : ORIGIN = 0x600E1000 , LENGTH = 0x000CB000    /* QSPI-FLASH*/
  SYSRAM (xrw)  : ORIGIN = 0x10004000 , LENGTH = 0x0000B8F0    /* SYSRAM FOR KERNEL */
  PSRAM (xrw)   : ORIGIN = 0x30002000 , LENGTH = 0x00238D00    /* External PSRAM */
}

/*
 * For external ram use something like:

  RAM (xrw) : ORIGIN = 0x68000000, LENGTH = 64K

 */
