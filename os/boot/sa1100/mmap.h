/* Addresses for all SA1100 variants.
 */

/* Physical addresses -- can't be changed */
#define PhysStatic0Base	0x00000000
#define PhysStatic1Base	0x08000000
#define PhysStatic2Base	0x10000000
#define PhysStatic3Base	0x18000000
#define PhysDRAMBase	0xC0000000
#define PhysDRAMSize	0x20000000

/* Remapped addresses that can't currently be set via `monpatch' 
 * These could be changed here if absolutely necessary, but could
 * result in inconsistencies between different boards
 */
#define	DRAMBase	0x00000000
#define DRAMSize	0x08000000
#define Static0Base	0x08000000
#define UBDRAMBase	0x10000000
#define UCDRAMBase	0x18000000

/* #define CodeBase	(Static0Base+0x4000) */

/* Some default offsets into the flash.
 * These can be patched in the binary if needed,
 * but the values below are the standard defaults
 */
#define DefPtabOfs	0x03e00
#define DefPtabSize	0x00200
#define DefMonOfs	0x00000
#define DefMonSize	0x03e00
#define DefAutobootOfs	0x00000
#define DefBootOfs	0x20000
