/*
 * Nikon D850 (EXPEED 5 / E5) validation prototype -- board constants.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Two classes of constants live here, and they are kept apart on purpose:
 *
 *  - D850-specific values measured from a locally supplied, already
 *    decrypted b1330_131.bin (D850 C:1.31):
 *
 *      vector table at 0x10000000, reset word -> 0x1037CBA8,
 *      exception vectors in a separate 0x4002xxxx bank,
 *      in-image MMU/init installer at 0x1035CE30,
 *      VBAR installed by that init at 0x10020000.
 *
 *  - inherited EXPEED-4 hypotheses used only for the assisted startup
 *    harness (stack, halt trampoline, low RAM, MPCore, UART).  They are
 *    NOT evidence about D850 hardware and are labelled as such:
 *
 *      RAM base 0x90000000, halt window at 0xFF0, MPCore at 0xE000E000.
 *
 * See ../docs/boot-chain.md for the bytes behind each D850 value.
 */

#ifndef HW_ARM_NIKON_D850_LAB_H
#define HW_ARM_NIKON_D850_LAB_H

/* Main image (b1330_131.bin, 0x187B6F6 bytes) load window.  The vector
 * table sits at 0x10000000 and the reset word points at 0x1037CBA8, so the
 * whole image is mapped in the flash window at this base. */
#define D850_PAYLOAD_BASE      0x10000000ULL

/* Reset word target.  This is a boot-ROM *service* stub, not a reset
 * handler: it is a `b/ldrb/str/pop` sled that expects r4 and a non-zero
 * stack from the real boot ROM (docs/boot-chain.md).  The prototype
 * therefore also exposes a host-assisted entry (D850_INIT_ADDR). */
#define D850_ENTRY_ADDR        0x1037CBA8ULL

/* First function of the in-image MMU/init installer.  Host-assisted entry
 * here reaches the TTBR/SCTLR programming and is the current experiment. */
#define D850_INIT_ADDR         0x1035CE30ULL

/* VBAR the init installer programs (literal at 0x1035d134). */
#define D850_VBAR_ADDR         0x10020000ULL

/* Separate handler/boot bank referenced by the exception vectors.  The
 * real contents are installed by the boot ROM; the prototype maps RAM here
 * and can place `bx lr` stubs for diagnostics. */
#define D850_HANDLER_BASE      0x40000000ULL
#define D850_HANDLER_SIZE      0x40000ULL
#define D850_BOOT_SCRATCH_REG  0x400E8450ULL
#define D850_BOOT_SCRATCH_END  0x400E845CULL

/* Reconstructed 0x40000000 window.  The in-image loader table describes two
 * segments that end at 0x40041CA8; the A-core init reads its TTBR/page-table
 * base from [0x400E8450], so a reconstructed run also needs a boot-argument
 * page and LPAE tables at that base. */
#define D850_BOOTROM_BASE      0x40000000ULL
#define D850_BOOTROM_SIZE      0x700000ULL
#define D850_BOOTARG_BASE      0x400E8000ULL
#define D850_BOOTARG_SIZE      0x1000ULL
#define D850_BOOTARG_SLOT      0x450ULL
#define D850_BOOT_TABLES_BASE  0x40708000ULL
#define D850_BOOT_TABLES_SIZE  0x2000ULL
#define D850_BOOTRAM_BASE      0x40700000ULL
#define D850_BOOTRAM_SIZE      0x10000ULL
#define D850_BOOT_TIMER_BASE   0x51340000ULL
#define D850_BOOT_TIMER_SIZE   0x1000ULL
#define D850_BOOT_STATUS_BASE  0x54000000ULL
#define D850_BOOT_STATUS_SIZE  0x3200000ULL
#define D850_BOOT_PEER_BASE    0x800E8000ULL
#define D850_BOOT_PEER_SIZE    0x1000ULL
#define D850_BOOT_STATUS2_BASE 0x51100000ULL
#define D850_BOOT_STATUS2_SIZE 0x1000ULL
#define D850_BOOT_ARG_DEFAULT  0x00708000ULL

/* The in-image init loads this literal and does `bx ip`; the reconstructed
 * stage-2 at 0x40028FF8 is Thumb, so the host-assisted run needs bit0 set
 * (the real boot ROM would patch the hand-off). */
#define D850_STAGE2_LITERAL_OFF 0x35D144ULL
#define D850_STAGE2_TARGET      0x40028FF8ULL

/* RAM window inherited from the EXPEED-4 work.  No D850 evidence pins the
 * DRAM base yet; it is used only to give the assisted entry a stack and a
 * place for the halt trampoline.  Do not read it as a D850 address. */
#define D850_RAM_BASE          0x90000000ULL
#define D850_RAM_MIN           (32ULL * 1024 * 1024)
#define D850_RAM_MAX           (512ULL * 1024 * 1024)

/* Low memory: the assisted harness stores the return address at 0xFF0.
 * The first 64 KiB are writable RAM; the halt window is the last word. */
#define D850_LOWRAM_BASE       0x00000000ULL
#define D850_LOWRAM_SIZE       (64ULL * 1024)
#define D850_HALT_TRAMPOLINE   0x00000FF0ULL

/* Flash / first-stage window.  D850 C:1.31 is 25.7 MB, so the default
 * window is 32 MiB; `-machine flash-size=` can shrink or grow it. */
#define D850_FLASH_BASE        0x10000000ULL
#define D850_FLASH_SIZE        (32ULL * 1024 * 1024)
#define D850_FLASH_SIZE_MAX    (64ULL * 1024 * 1024)

/* Optional second RAM bank at 0x20000000 (inherited page-table
 * experiment, kept for cross-model runs; off by default). */
#define D850_AUXRAM_BASE       0x20000000ULL

/* Legacy EXPEED-4 boot-chain constants.  They stay because the shared
 * overlay still supports the older `boot-mode=stub` path, but they are not
 * used by the D850 defaults and are not claimed for D850. */
#define D850_A1010_BASE        0x10018000ULL
#define D850_BOOT_STUB         0x9025D700ULL
#define D850_BX_SITE           0x9025D750ULL
#define D850_BX_LITERAL_OFF    0x25D7F8ULL
#define D850_BX_TARGET         0x1002CB8CULL
#define D850_BOOT_STUB_SP_DELTA 0x1000ULL
#define D850_BOOT_MMU_TRAMPOLINE 0x00001000ULL
#define D850_BOOT_MMU_TABLE_SIZE 0x4000ULL
#define D850_BOOT_MMU_TABLE_DELTA 0x10000ULL
#define D850_CONFIG_SLOT0      0x100F48C0ULL
#define D850_VECTOR_OFF        0x20000ULL
#define D850_VECTOR_LEN        0x60ULL

/* MPCore private block.  Address measured on the older EXPEED-4 prototype
 * (first MMIO was a write to 0xE000E180); retained as the default GIC /
 * generic-timer window for the D850 runs, with -machine mpcore-base= to
 * override it once D850 makes its own access. */
#define D850_MPCORE_BASE       0xE000E000ULL
#define D850_GIC_NUM_IRQ       96

/* Candidate byte-register serial block (inherited; no D850 bytes observed
 * yet). */
#define D850_UART_BASE         0x80005000ULL
#define D850_UART_SIZE         0x100

/* EXPEED register blocks used by the inherited diagnostic io-stub. */
#define D850_IO_A2_BASE        0xA2000000ULL
#define D850_IO_A2_SIZE        0x10000
#define D850_IO_AD_BASE        0xAD000000ULL
#define D850_IO_AD_SIZE        0x100000
#define D850_IO_AE_BASE        0xAE000000ULL
#define D850_IO_AE_SIZE        0x10000
#define D850_IO_A340_BASE      0xA3400000ULL
#define D850_IO_A340_SIZE      0x100000
#define D850_IO_AE_READY       0xAE00060CULL

/* Catch-all diagnostic window used when strict=off. */
#define D850_PROBE_PRIORITY    (-1000)
#define D850_PROBE_SIZE        0x100000000ULL

#endif /* HW_ARM_NIKON_D850_LAB_H */
