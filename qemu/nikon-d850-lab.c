/*
 * Nikon D850 (EXPEED 5 / E5) validation prototype machine.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This is an *experimental validation prototype*, not a D850 emulator and
 * not a complete EXPEED 5 model.  It exists to answer a small, ordered set
 * of questions about a locally supplied, already decrypted b1330 module:
 *
 *   D0  does the image load at 0x10000000 and start at the reset word
 *       0x1037CBA8 with the expected CPU state?
 *   D1  what does the reset stub actually need from the boot ROM?
 *   D2  does the in-image init installer at 0x1035CE30 reach MMU enable
 *       and which registers does it program?
 *   D3  what is the first missing fixture after that point?
 *
 * The machine therefore favours *diagnostics* over plausible-looking data:
 * an unmodelled access is logged with PC/address/size/direction instead of
 * silently returning zero.  strict=on keeps the same window but records the
 * first access as "d850-strict:" and stops the run, so a strict run always
 * ends either at the assisted trampoline or at an explicit diagnostic.
 *
 * Inputs (nothing is downloaded or embedded):
 *   -kernel  FILE   the decrypted main image, normally b1330_131.bin
 *   -machine nikon-d850,payload=FILE,payload-base=0x10000000,entry=ADDR
 *
 * The default profile maps the payload into the flash window at 0x10000000
 * and enters the reset word.  The EXPEED-4 RAM window at 0x90000000 is used
 * only for the assisted stack and the halt trampoline.  See
 * nikon-d850-lab.h and ../docs/boot-chain.md.
 */

#include "qemu/osdep.h"

#include "qapi/error.h"
#include "qapi/visitor.h"
#include "qemu/bswap.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qemu/units.h"

#include "chardev/char-fe.h"
#include "exec/cpu-common.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/machines-qom.h"
#include "hw/core/boards.h"
#include "hw/core/clock.h"
#include "hw/core/cpu.h"
#include "hw/core/loader.h"
#include "hw/core/qdev-clock.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/sysbus.h"
#include "hw/cpu/a9mpcore.h"
#include "hw/intc/arm_gic.h"
#include "qom/object.h"
#include "system/cpus.h"
#include "system/memory.h"
#include "system/reset.h"
#include "system/runstate.h"
#include "system/system.h"
#include "target/arm/cpu.h"
#include "target/arm/cpu-qom.h"

#include "nikon-d850-lab.h"

#define TYPE_NIKON_D850_MACHINE MACHINE_TYPE_NAME("nikon-d850")

/* qdev GPIO input order on every ARM CPU.  Kept local because the old
 * include/hw/arm/arm.h that carried this enum no longer exists. */
enum {
    D850_CPU_IRQ = 0,
    D850_CPU_FIQ,
    D850_CPU_VIRQ,
    D850_CPU_VFIQ,
};

struct D850IoStub;

typedef struct NikonD850MachineState {
    MachineState parent;
    MemoryRegion lowram;        /* vectors / boot globals at 0x0          */
    MemoryRegion halt;          /* return trampoline: fetch = clean exit   */
    MemoryRegion flash;         /* RAM-backed, read/write in this prototype */
    MemoryRegion auxram;        /* optional second RAM bank at 0x20000000  */
    MemoryRegion aux2;          /* optional handler RAM (E5: 0x40000000)  */
    MemoryRegion e6sram;        /* E6 boot SRAM at 0x04000000              */
    MemoryRegion e6stack;       /* E6 stacks at 0xC0000000                 */
    MemoryRegion probe;         /* catch-all diagnostic window             */
    MemoryRegion vbr;           /* virtual 0x40000000 boot-ROM window      */
    MemoryRegion uart;          /* candidate byte-register serial block    */

    CharFrontend chr;

    char *payload;
    char *flash_path;
    char *boot;                 /* "entry" (M0) or "stub" (M1)             */
    uint64_t entry;             /* UINT64_MAX means "use the inferred one" */
    uint64_t payload_base;      /* where the payload image is loaded        */
    uint64_t a1010_base;        /* where the a1010 module is mapped        */
    uint32_t a1010_size;        /* bytes actually loaded, 0 = none         */
    uint64_t flash_size;        /* flash window size (default 8 MiB)       */
    uint64_t auxram_size;       /* 0 = no bank at 0x20000000               */
    uint64_t aux2_base;         /* optional handler RAM base (0 = off)     */
    uint64_t aux2_size;         /* optional handler RAM size               */
    uint64_t mmu_table;         /* explicit identity-table address (0=auto)*/
    bool thumb_fix;             /* patch the bx literal to Thumb state     */
    bool vector_fix;            /* install a minimal ARM vector table      */
    uint64_t veneer_slot;       /* payload offset of one veneer to patch   */
    bool loader_copies;         /* apply the descriptor-table copy records */
    bool mmu_bringup;           /* emulate the missing boot-ROM MMU setup  */
    bool io_stub;               /* map A2/AD/AE/A340 register-block stubs  */
    bool e6_prep;               /* patch the D780 page table + map stacks  */
    bool vbr_on;                /* virtual 0x40000000 boot-ROM stubs       */
    bool e5_tables;             /* LPAE identity fixture for the E5 boot ROM */
    struct D850IoStub *io_blocks[4];
    uint64_t mpcore_base;       /* GIC/timers/watchdog block address       */
    bool strict;
    uint32_t uart_status;

    uint64_t probe_events;
    uint64_t uart_events;
    uint64_t a1010_io_events;

    /* Optional framebuffer dump path.  No address is guessed: the window is
     * inert until -machine lcd-base=... is supplied. */
    uint64_t lcd_base;
    uint32_t lcd_width;
    uint32_t lcd_height;
    uint32_t lcd_stride;
    char *lcd_format;
    char *lcd_out;
    QEMUTimer *lcd_timer;
} NikonD850MachineState;

DECLARE_INSTANCE_CHECKER(NikonD850MachineState, NIKON_D850_MACHINE,
                         TYPE_NIKON_D850_MACHINE)

/* ------------------------------------------------------------------ */
/* Diagnostics                                                         */
/* ------------------------------------------------------------------ */

static uint64_t d850_guest_pc(void)
{
    CPUState *cs = current_cpu;

    if (!cs) {
        return 0;
    }
    return ARM_CPU(cs)->env.regs[15];
}

/* True when the current instruction belongs to the a1010 module.  The M1
 * acceptance point is the first unmodelled access issued from this range:
 * b1010's own diagnostics are expected, a1010's are the next stage. */
static bool d850_pc_in_a1010(NikonD850MachineState *s)
{
    uint64_t pc = d850_guest_pc();

    return s->a1010_size != 0 &&
           pc >= s->a1010_base &&
           pc - s->a1010_base < s->a1010_size;
}

/*
 * One window, two contracts.  By default the access is logged and the guest
 * keeps running (the "collect a whole sequence" mode).  With strict=on the
 * first access is still logged -- but with the "d850-strict:" prefix, so the
 * address is not lost -- and the run is stopped instead of letting the guest
 * continue into an exception handler that could look like a clean return.
 */
static uint64_t d850_probe_read(void *opaque, hwaddr addr, unsigned size)
{
    NikonD850MachineState *s = opaque;

    if (s->strict) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-strict: pc=0x%08" PRIx64
                      " rd addr=0x%08" HWADDR_PRIx " size=%u"
                      " (first unmodelled access, stopping)\n",
                      d850_guest_pc(), addr, size);
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
        return 0;
    }

    if (d850_pc_in_a1010(s)) {
        if (s->a1010_io_events < 64 || (s->a1010_io_events % 1024) == 0) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "d850-a1010-io: pc=0x%08" PRIx64
                          " rd addr=0x%08" HWADDR_PRIx " size=%u"
                          " (unmodelled read from a1010, returning 0)\n",
                          d850_guest_pc(), addr, size);
        }
        s->a1010_io_events++;
        return 0;
    }

    if (s->probe_events < 512 || (s->probe_events % 4096) == 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-probe: pc=0x%08" PRIx64
                      " rd addr=0x%08" HWADDR_PRIx " size=%u"
                      " (unmodelled read, returning 0)\n",
                      d850_guest_pc(), addr, size);
    }
    s->probe_events++;
    return 0;
}

static void d850_probe_write(void *opaque, hwaddr addr, uint64_t value,
                             unsigned size)
{
    NikonD850MachineState *s = opaque;

    if (s->strict) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-strict: pc=0x%08" PRIx64
                      " wr addr=0x%08" HWADDR_PRIx " size=%u value=0x%08" PRIx64
                      " (first unmodelled access, stopping)\n",
                      d850_guest_pc(), addr, size, value);
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
        return;
    }

    if (d850_pc_in_a1010(s)) {
        if (s->a1010_io_events < 64 || (s->a1010_io_events % 1024) == 0) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "d850-a1010-io: pc=0x%08" PRIx64
                          " wr addr=0x%08" HWADDR_PRIx " size=%u value=0x%08"
                          PRIx64 " (unmodelled write from a1010)\n",
                          d850_guest_pc(), addr, size, value);
        }
        s->a1010_io_events++;
        return;
    }

    if (s->probe_events < 512 || (s->probe_events % 4096) == 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-probe: pc=0x%08" PRIx64
                      " wr addr=0x%08" HWADDR_PRIx " size=%u value=0x%08" PRIx64
                      " (unmodelled write)\n",
                      d850_guest_pc(), addr, size, value);
    }
    s->probe_events++;
}

static const MemoryRegionOps d850_probe_ops = {
    .read = d850_probe_read,
    .write = d850_probe_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .impl.min_access_size = 1,
    .impl.max_access_size = 8,
};

/* ------------------------------------------------------------------ */
/* EXPEED 4 register-block stub (diagnostic)                            */
/* ------------------------------------------------------------------ */
/*
 * The older EXPEED-4 prototype's reset handler (and the D3300 one) touches
 * A2/AD/AE/A340 blocks that are not documented yet.  `io-stub=on` maps them
 * as simple register files and reports 0xAE00060C as ready, which is the bit
 * that handler polls 8048 times.  D850 defaults do not enable this; it is
 * scaffolding for cross-model tracing, not a model of the hardware.
 */
typedef struct D850IoStub {
    MemoryRegion mr;
    uint64_t base;
    uint32_t size;
    uint32_t *regs;
} D850IoStub;

static uint64_t d850_io_stub_read(void *opaque, hwaddr addr, unsigned size)
{
    D850IoStub *s = opaque;

    if (s->base + addr == D850_IO_AE_READY) {
        return 1;
    }
    if (addr + size > s->size) {
        return 0;
    }
    return s->regs[addr >> 2];
}

static void d850_io_stub_write(void *opaque, hwaddr addr, uint64_t value,
                               unsigned size)
{
    D850IoStub *s = opaque;

    if (addr + size > s->size) {
        return;
    }
    s->regs[addr >> 2] = (uint32_t)value;
}

static const MemoryRegionOps d850_io_stub_ops = {
    .read = d850_io_stub_read,
    .write = d850_io_stub_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
};

/* ------------------------------------------------------------------ */
/* Optional framebuffer dump                                           */
/* ------------------------------------------------------------------ */
/*
 * A host-side observation tool, not a display model.  It reads the
 * configured window from guest RAM and writes a PPM.  The window is inert
 * until -machine lcd-base=... is supplied, so no framebuffer address is
 * guessed.  Once G1 of docs/gui-cmos.md observes the real address and
 * format, only the machine options change.
 */

static void d850_lcd_dump(NikonD850MachineState *s)
{
    MachineState *machine = MACHINE(s);
    uint32_t bpp, stride, x, y;
    uint64_t size;
    uint8_t *ram, *row;
    FILE *f;

    if (!s->lcd_base || !s->lcd_out || !s->lcd_format) {
        return;
    }
    if (s->lcd_width == 0 || s->lcd_height == 0 ||
        s->lcd_width > 4096 || s->lcd_height > 4096) {
        return;
    }

    if (!strcmp(s->lcd_format, "rgb565")) {
        bpp = 2;
    } else if (!strcmp(s->lcd_format, "rgb888")) {
        bpp = 3;
    } else {
        bpp = 4;
    }
    stride = s->lcd_stride ? s->lcd_stride : s->lcd_width * bpp;
    size = (uint64_t)stride * s->lcd_height;
    if (s->lcd_base < D850_RAM_BASE || size > machine->ram_size ||
        s->lcd_base - D850_RAM_BASE > machine->ram_size - size) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-lcd: window 0x%08" PRIx64
                      " (%" PRIu64 " bytes) is outside the RAM window\n",
                      s->lcd_base, size);
        return;
    }

    ram = memory_region_get_ram_ptr(machine->ram) +
          (s->lcd_base - D850_RAM_BASE);
    f = fopen(s->lcd_out, "wb");
    if (!f) {
        qemu_log_mask(LOG_GUEST_ERROR, "d850-lcd: cannot open '%s'\n",
                      s->lcd_out);
        return;
    }
    fprintf(f, "P6\n%u %u\n255\n", s->lcd_width, s->lcd_height);
    for (y = 0; y < s->lcd_height; y++) {
        row = ram + (uint64_t)y * stride;
        for (x = 0; x < s->lcd_width; x++) {
            uint8_t rgb[3];

            if (bpp == 2) {
                uint16_t v = row[x * 2] | (row[x * 2 + 1] << 8);
                rgb[0] = (uint8_t)(((v >> 11) & 0x1f) * 255 / 31);
                rgb[1] = (uint8_t)(((v >> 5) & 0x3f) * 255 / 63);
                rgb[2] = (uint8_t)((v & 0x1f) * 255 / 31);
            } else if (bpp == 3) {
                const uint8_t *p = row + x * 3;
                rgb[0] = p[0];
                rgb[1] = p[1];
                rgb[2] = p[2];
            } else {
                /* little-endian ARGB8888: memory order B, G, R, A */
                const uint8_t *p = row + x * 4;
                rgb[0] = p[2];
                rgb[1] = p[1];
                rgb[2] = p[0];
            }
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    qemu_log_mask(LOG_GUEST_ERROR,
                  "d850-lcd: dumped %ux%u (%s, stride %u) from 0x%08" PRIx64
                  " to %s\n", s->lcd_width, s->lcd_height, s->lcd_format,
                  stride, s->lcd_base, s->lcd_out);
}

static void d850_lcd_tick(void *opaque)
{
    NikonD850MachineState *s = opaque;

    d850_lcd_dump(s);
    timer_mod(s->lcd_timer,
              qemu_clock_get_ms(QEMU_CLOCK_REALTIME) + 2000);
}

/* ------------------------------------------------------------------ */
/* Return trampoline                                                   */
/* ------------------------------------------------------------------ */
/*
 * The inferred entry is a called function.  The assisted startup gives it
 * LR = D850_HALT_TRAMPOLINE; when it returns, the fetch from this
 * four-byte window is both the "entry finished" evidence and a clean
 * shutdown request, so a run terminates instead of spinning.
 *
 * A fault, however, enters the guest vector table (all zeros here), slides
 * through low memory and can reach the same 0xFF0 window from abort mode
 * with SP=0.  That would look exactly like a successful M0, so the state is
 * checked and recorded explicitly.
 */

static bool d850_halt_state_is_clean(NikonD850MachineState *s, ARMCPU *cpu)
{
    MachineState *machine = MACHINE(s);
    uint32_t expected_sp = (uint32_t)(D850_RAM_BASE + machine->ram_size - 0x100);
    uint32_t mode = cpu->env.uncached_cpsr & CPSR_M;

    return mode == ARM_CPU_MODE_SVC &&
           cpu->env.regs[14] == (uint32_t)D850_HALT_TRAMPOLINE &&
           cpu->env.regs[13] == expected_sp;
}

static uint64_t d850_halt_read(void *opaque, hwaddr addr, unsigned size)
{
    NikonD850MachineState *s = opaque;

    d850_lcd_dump(s);
    if (current_cpu) {
        ARMCPU *cpu = ARM_CPU(current_cpu);
        bool clean = d850_halt_state_is_clean(s, cpu);
        uint32_t mode = cpu->env.uncached_cpsr & CPSR_M;

        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-halt: pc=0x%08" PRIx64
                      " r0=0x%08" PRIx32 " r1=0x%08" PRIx32
                      " sp=0x%08" PRIx32 " lr=0x%08" PRIx32
                      " return_ok=%s mode=0x%02" PRIx32 " %s\n",
                      d850_guest_pc(),
                      cpu->env.regs[0], cpu->env.regs[1],
                      cpu->env.regs[13], cpu->env.regs[14],
                      clean ? "true" : "false", mode,
                      clean ? "firmware returned to the assisted trampoline"
                            : "unexpected state (exception or wild branch "
                              "reached the trampoline)");
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-halt: firmware returned to the assisted trampoline\n");
    }
    qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
    return 0;
}

static void d850_halt_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
    d850_halt_read(opaque, addr, size);
}

static const MemoryRegionOps d850_halt_ops = {
    .read = d850_halt_read,
    .write = d850_halt_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

/* ------------------------------------------------------------------ */
/* Candidate serial block                                              */
/* ------------------------------------------------------------------ */
/*
 * The driver slice at file offset 0x27CC00..0x27D304 reaches a byte-wide
 * register block at 0x80005010..0x80005014.  We model the smallest useful
 * contract: a TX byte at +0x10, a status byte at +0x11, and a second
 * status/control byte at +0x14.  Everything else is logged and returns
 * zero.  A wrong decode shows up as a probe log line and a stalled poll,
 * which is exactly the information the next iteration needs.
 */

static uint64_t d850_uart_read(void *opaque, hwaddr addr, unsigned size)
{
    NikonD850MachineState *s = opaque;
    uint64_t value = 0;

    switch (addr) {
    case 0x11:
    case 0x14:
        value = s->uart_status;
        break;
    default:
        value = 0;
        break;
    }

    if (s->uart_events < 128) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-uart: pc=0x%08" PRIx64
                      " rd +0x%02" HWADDR_PRIx " size=%u -> 0x%02" PRIx64 "\n",
                      d850_guest_pc(), addr, size, value);
    }
    s->uart_events++;
    return value;
}

static void d850_uart_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
    NikonD850MachineState *s = opaque;

    if (addr == 0x10) {
        uint8_t ch = value & 0xff;

        if (qemu_chr_fe_backend_connected(&s->chr)) {
            qemu_chr_fe_write_all(&s->chr, &ch, 1);
        }
        qemu_log_mask(LOG_TRACE, "d850-uart: tx 0x%02x ('%c')\n",
                      ch, (ch >= 0x20 && ch < 0x7f) ? ch : '.');
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "d850-uart: pc=0x%08" PRIx64
                      " wr +0x%02" HWADDR_PRIx " size=%u value=0x%02" PRIx64 "\n",
                      d850_guest_pc(), addr, size, value & 0xff);
    }
    s->uart_events++;
}

static const MemoryRegionOps d850_uart_ops = {
    .read = d850_uart_read,
    .write = d850_uart_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 4,
    .impl.min_access_size = 1,
    .impl.max_access_size = 4,
};

/* ------------------------------------------------------------------ */
/* Machine properties                                                  */
/* ------------------------------------------------------------------ */

static char *nikon_d850_get_payload(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return g_strdup(s->payload);
}

static void nikon_d850_set_payload(Object *obj, const char *value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    g_free(s->payload);
    s->payload = g_strdup(value);
}

static char *nikon_d850_get_flash(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return g_strdup(s->flash_path);
}

static void nikon_d850_set_flash(Object *obj, const char *value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    g_free(s->flash_path);
    s->flash_path = g_strdup(value);
}

static void nikon_d850_get_entry(Object *obj, Visitor *v, const char *name,
                                 void *opaque, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->entry;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_entry(Object *obj, Visitor *v, const char *name,
                                 void *opaque, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->entry = value;
}

static void nikon_d850_get_payload_base(Object *obj, Visitor *v,
                                        const char *name, void *opaque,
                                        Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->payload_base;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_payload_base(Object *obj, Visitor *v,
                                        const char *name, void *opaque,
                                        Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->payload_base = value;
}

static bool nikon_d850_get_strict(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->strict;
}

static void nikon_d850_set_strict(Object *obj, bool value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->strict = value;
}

static char *nikon_d850_get_boot(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return g_strdup(s->boot);
}

static void nikon_d850_set_boot(Object *obj, const char *value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    g_free(s->boot);
    s->boot = g_strdup(value);
}

static void nikon_d850_get_a1010_base(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->a1010_base;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_a1010_base(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->a1010_base = value;
}

static bool nikon_d850_get_thumb_fix(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->thumb_fix;
}

static void nikon_d850_set_thumb_fix(Object *obj, bool value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->thumb_fix = value;
}

static bool nikon_d850_get_mmu_bringup(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->mmu_bringup;
}

static void nikon_d850_set_mmu_bringup(Object *obj, bool value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->mmu_bringup = value;
}

static void nikon_d850_get_mpcore_base(Object *obj, Visitor *v,
                                       const char *name, void *opaque,
                                       Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->mpcore_base;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_mpcore_base(Object *obj, Visitor *v,
                                       const char *name, void *opaque,
                                       Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->mpcore_base = value;
}

static void nikon_d850_get_flash_size(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->flash_size;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_flash_size(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->flash_size = value;
}

static void nikon_d850_get_auxram_size(Object *obj, Visitor *v,
                                       const char *name, void *opaque,
                                       Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->auxram_size;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_auxram_size(Object *obj, Visitor *v,
                                       const char *name, void *opaque,
                                       Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->auxram_size = value;
}

static void nikon_d850_get_aux2_base(Object *obj, Visitor *v,
                                     const char *name, void *opaque,
                                     Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->aux2_base;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_aux2_base(Object *obj, Visitor *v,
                                     const char *name, void *opaque,
                                     Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->aux2_base = value;
}

static void nikon_d850_get_aux2_size(Object *obj, Visitor *v,
                                     const char *name, void *opaque,
                                     Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->aux2_size;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_aux2_size(Object *obj, Visitor *v,
                                     const char *name, void *opaque,
                                     Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->aux2_size = value;
}

static void nikon_d850_get_mmu_table(Object *obj, Visitor *v,
                                     const char *name, void *opaque,
                                     Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->mmu_table;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_mmu_table(Object *obj, Visitor *v,
                                     const char *name, void *opaque,
                                     Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->mmu_table = value;
}

static bool nikon_d850_get_vector_fix(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->vector_fix;
}

static void nikon_d850_set_vector_fix(Object *obj, bool value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->vector_fix = value;
}

static void nikon_d850_get_veneer_slot(Object *obj, Visitor *v,
                                       const char *name, void *opaque,
                                       Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->veneer_slot;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_veneer_slot(Object *obj, Visitor *v,
                                       const char *name, void *opaque,
                                       Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->veneer_slot = value;
}

static bool nikon_d850_get_loader_copies(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->loader_copies;
}

static void nikon_d850_set_loader_copies(Object *obj, bool value,
                                         Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->loader_copies = value;
}

static bool nikon_d850_get_io_stub(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->io_stub;
}

static void nikon_d850_set_io_stub(Object *obj, bool value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->io_stub = value;
}

static bool nikon_d850_get_vbr(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->vbr_on;
}

static void nikon_d850_set_vbr(Object *obj, bool value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->vbr_on = value;
}

static bool nikon_d850_get_e5_tables(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->e5_tables;
}

static void nikon_d850_set_e5_tables(Object *obj, bool value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->e5_tables = value;
}

static bool nikon_d850_get_e6_prep(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return s->e6_prep;
}

static void nikon_d850_set_e6_prep(Object *obj, bool value, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->e6_prep = value;
}

static void nikon_d850_get_lcd_base(Object *obj, Visitor *v,
                                    const char *name, void *opaque,
                                    Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value = s->lcd_base;

    visit_type_uint64(v, name, &value, errp);
}

static void nikon_d850_set_lcd_base(Object *obj, Visitor *v,
                                    const char *name, void *opaque,
                                    Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint64_t value;

    if (!visit_type_uint64(v, name, &value, errp)) {
        return;
    }
    s->lcd_base = value;
}

static void nikon_d850_get_lcd_width(Object *obj, Visitor *v,
                                     const char *name, void *opaque,
                                     Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint32_t value = s->lcd_width;

    visit_type_uint32(v, name, &value, errp);
}

static void nikon_d850_set_lcd_width(Object *obj, Visitor *v,
                                     const char *name, void *opaque,
                                     Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint32_t value;

    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }
    s->lcd_width = value;
}

static void nikon_d850_get_lcd_height(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint32_t value = s->lcd_height;

    visit_type_uint32(v, name, &value, errp);
}

static void nikon_d850_set_lcd_height(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint32_t value;

    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }
    s->lcd_height = value;
}

static void nikon_d850_get_lcd_stride(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint32_t value = s->lcd_stride;

    visit_type_uint32(v, name, &value, errp);
}

static void nikon_d850_set_lcd_stride(Object *obj, Visitor *v,
                                      const char *name, void *opaque,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);
    uint32_t value;

    if (!visit_type_uint32(v, name, &value, errp)) {
        return;
    }
    s->lcd_stride = value;
}

static char *nikon_d850_get_lcd_format(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return g_strdup(s->lcd_format);
}

static void nikon_d850_set_lcd_format(Object *obj, const char *value,
                                      Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    g_free(s->lcd_format);
    s->lcd_format = g_strdup(value);
}

static char *nikon_d850_get_lcd_out(Object *obj, Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    return g_strdup(s->lcd_out);
}

static void nikon_d850_set_lcd_out(Object *obj, const char *value,
                                   Error **errp)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    g_free(s->lcd_out);
    s->lcd_out = g_strdup(value);
}

/* ------------------------------------------------------------------ */
/* M1 boot-stub assistance                                             */
/* ------------------------------------------------------------------ */
/*
 * Payload offset 0x20000 is the address b1010's init installs as VBAR
 * (0x90020000).  Without the real boot ROM that area contains Thumb code, so
 * any exception lands on data.  `vector-fix=on` installs a minimal ARM vector
 * table: SVC returns, IRQ/FIQ return, all faults spin.  This is diagnostic
 * scaffolding, not a reconstruction of the firmware's handlers.
 */
static void d850_apply_vector_fix(uint8_t *data, gsize size)
{
    static const uint32_t handlers[8] = {
        0xEAFFFFFE,             /* reset:        b .                    */
        0xEAFFFFFE,             /* undefined:    b .                    */
        0xE14F1000,             /* SVC:          mrs r1, spsr           */
        0xE3811020,             /*               orr r1, r1, #0x20      */
        0xE16FF001,             /*               msr spsr_cxsf, r1      */
        0xE1B0F00E,             /*               movs pc, lr            */
        0xE25EF004,             /* IRQ:          subs pc, lr, #4        */
        0xE25EF004,             /* FIQ:          subs pc, lr, #4        */
    };
    int i;

    if (size < D850_VECTOR_OFF + D850_VECTOR_LEN) {
        warn_report("nikon-d850: payload is too small for the vector fix");
        return;
    }
    for (i = 0; i < 8; i++) {
        /* every vector is `b +0x38`: vector + 8 + 0x38 == vector + 0x40 */
        stl_le_p(data + D850_VECTOR_OFF + i * 4, 0xEA00000E);
        stl_le_p(data + D850_VECTOR_OFF + 0x40 + i * 4, handlers[i]);
    }
    memset(data + D850_VECTOR_OFF + 0x20, 0, 0x20);
    info_report("nikon-d850: minimal ARM vector table at 0x%08" PRIx64,
                (uint64_t)(D850_RAM_BASE + D850_VECTOR_OFF));
}

/*
 * The b1010 flash image contains veneer tables of the form
 *     04 f0 1f e5   ldr pc, [pc, #-4]
 *     <target>
 * where <target> is a flash address.  On real hardware the loader relocates
 * these slots and sets bit0 for Thumb functions; raw image targets are even,
 * so QEMU enters ARM state and executes Thumb bytes as garbage.  A blanket
 * scan is too broad (it also hits data and ARM entries), so the diagnostic
 * option takes one explicit file offset and sets bit0 there.
 */
static void d850_apply_veneer_slot(NikonD850MachineState *s, uint8_t *data,
                                   gsize size)
{
    uint64_t off = s->veneer_slot;
    uint32_t target;

    if (off + 4 > size) {
        warn_report("nikon-d850: veneer-slot 0x%08" PRIx64
                    " is outside the payload", off);
        return;
    }
    target = ldl_le_p(data + off);
    if (target < D850_FLASH_BASE || target >= D850_FLASH_BASE + size ||
        (target & 1)) {
        warn_report("nikon-d850: veneer-slot 0x%08" PRIx64 " is 0x%08"
                    PRIx32 " (not an even flash pointer); skipped",
                    off, target);
        return;
    }
    stl_le_p(data + off, target | 1);
    info_report("nikon-d850: veneer slot 0x%08" PRIx64 ": 0x%08" PRIx32
                " -> 0x%08" PRIx32, off, target, target | 1);
}

/*
 * The descriptor table at b1010 0x25D760..0x25D7B8 contains four
 * (dest, src, size) copy records.  The chains are exact:
 *   dest A + size A = dest B,  src A + size A = src B,
 *   src B + size B  = 0x1003FD16 (the value stored at 0x25D7D0),
 *   src C + size C  = 0x100F48CC (just past the config slots).
 * The destinations are zero-filled in the RAM image, so the boot ROM must
 * apply these copies before entering the payload.  `loader-copies=on`
 * reproduces them in the in-memory payload; because copy C runs past the
 * 20 MiB file image, the buffer is extended with zeros first.
 */
static const struct D850LoaderCopy {
    uint64_t dest;
    uint64_t src;
    uint32_t size;
} d850_loader_copies[] = {
    { 0x9128DB64, 0x10000000, 0x030344 },   /* A: jump table + code        */
    { 0x912BDEA8, 0x10030344, 0x00F9D2 },   /* B: continuation of A        */
    { 0x9132EC00, 0x100A1480, 0x05344C },   /* C: data block before config */
    { 0x9132EBF4, 0x100A1094, 0x000004 },   /* D: Thumb stub "movs r0,#0; bx lr" */
};

static void d850_apply_loader_copies(gchar **data, gsize *size)
{
    gsize need = *size;
    int i;

    for (i = 0; i < (int)ARRAY_SIZE(d850_loader_copies); i++) {
        uint64_t end = d850_loader_copies[i].dest
                       + d850_loader_copies[i].size - D850_RAM_BASE;
        if (end > need) {
            need = end;
        }
    }
    if (need > *size) {
        *data = g_realloc(*data, need);
        memset(*data + *size, 0, need - *size);
        *size = need;
    }
    for (i = 0; i < (int)ARRAY_SIZE(d850_loader_copies); i++) {
        uint64_t dest = d850_loader_copies[i].dest;
        uint64_t src = d850_loader_copies[i].src;
        uint32_t len = d850_loader_copies[i].size;
        uint64_t doff = dest - D850_RAM_BASE;
        uint64_t soff = src - D850_FLASH_BASE;

        if (soff + len > *size || doff + len > *size) {
            warn_report("nikon-d850: loader copy 0x%08" PRIx64
                        " -> 0x%08" PRIx64 " does not fit the payload",
                        src, dest);
            continue;
        }
        memcpy(*data + doff, *data + soff, len);
        info_report("nikon-d850: loader copy 0x%08" PRIx64 " -> 0x%08"
                    PRIx64 " (%" PRIu32 " bytes)", src, dest, len);
    }
}

/*
 * The measured hand-off is `bx ip` at 0x9025D750 with ip = 0x1002CB8C,
 * while file 0x14B8C is Thumb-2 code.  This is legacy EXPEED-4 support;
 * the D850 profile never reaches it.  The payload is read into a host
 * buffer, the 32-bit literal at b1010 file offset 0x25D7F8 is corrected
 * to 0x1002CB8D, and the patched image is registered as a ROM blob.  This
 * is the in-memory equivalent of the firmware repack described in
 * docs/findings.md: no instruction bytes change and the file on disk is
 * never touched.
 *
 * The patch has to happen here rather than through the loaded image:
 * load_image_targphys() registers a ROM blob whose bytes are only written
 * at machine reset, after nikon_d850_init() has returned.
 */
static void d850_load_payload(NikonD850MachineState *s, const char *path,
                              uint64_t addr, uint64_t max_size)
{
    gchar *data = NULL;
    gsize size = 0;
    GError *err = NULL;

    if (!g_file_get_contents(path, &data, &size, &err)) {
        error_report("nikon-d850: cannot load payload '%s': %s", path,
                     err ? err->message : "unknown error");
        g_clear_error(&err);
        exit(1);
    }
    if (size > max_size) {
        error_report("nikon-d850: payload '%s' (%" G_GSIZE_FORMAT
                     " bytes) does not fit in RAM (%" PRIu64 " bytes)",
                     path, size, max_size);
        g_free(data);
        exit(1);
    }

    if (s->loader_copies) {
        d850_apply_loader_copies(&data, &size);
    }

    if (s->e6_prep && size >= 0x14000) {
        int i;
        uint32_t patched = 0;

        /* D780 startup uses the table at payload+0x10000 (0x10030000);
         * the image stores bare 1 MiB physical addresses there, so turn
         * each into an identity section descriptor (AP=rw, C/B on). */
        for (i = 0; i < 4096; i++) {
            uint32_t w = ldl_le_p((uint8_t *)data + 0x10000 + i * 4);

            if ((w & 3) == 0) {
                stl_le_p((uint8_t *)data + 0x10000 + i * 4, w | 0x0C0E);
                patched++;
            }
        }
        info_report("nikon-d850: E6 page table patched at +0x10000: "
                    "%" PRIu32 " sections", patched);
    }

    if (!strcmp(s->boot, "stub") && s->thumb_fix &&
        size >= D850_BX_LITERAL_OFF + sizeof(uint32_t)) {
        uint32_t word = ldl_le_p(data + D850_BX_LITERAL_OFF);

        if (word == (uint32_t)D850_BX_TARGET) {
            stl_le_p(data + D850_BX_LITERAL_OFF, word | 1);
            info_report("nikon-d850: M1 thumb fix: [0x%08" PRIx64
                        "] 0x%08" PRIx64 " -> 0x%08" PRIx64,
                        (uint64_t)(D850_RAM_BASE + D850_BX_LITERAL_OFF),
                        (uint64_t)D850_BX_TARGET,
                        (uint64_t)D850_BX_TARGET | 1);
        } else {
            warn_report("nikon-d850: bx literal at payload offset 0x%08"
                        PRIx64 " is 0x%08" PRIx32 ", expected 0x%08" PRIx64
                        "; thumb-fix skipped",
                        (uint64_t)D850_BX_LITERAL_OFF, word,
                        (uint64_t)D850_BX_TARGET);
        }
    }

    if (s->veneer_slot) {
        d850_apply_veneer_slot(s, (uint8_t *)data, size);
    }
    if (s->vector_fix) {
        d850_apply_vector_fix((uint8_t *)data, size);
    }

    rom_add_blob_fixed(path, data, size, addr);
    g_free(data);
}

/*
 * Load the flash-window image through the same patch pipeline as the payload
 * (currently `veneer-slot`), so flash-resident code sees the same relocation.
 * Registers a ROM blob, like load_image_targphys did, and records the size for
 * the `d850-a1010-io` range check.
 */
static void d850_load_flash(NikonD850MachineState *s, const char *path,
                            uint64_t addr, uint64_t max_size)
{
    gchar *data = NULL;
    gsize size = 0;
    GError *err = NULL;

    if (!g_file_get_contents(path, &data, &size, &err)) {
        error_report("nikon-d850: cannot load flash image '%s': %s", path,
                     err ? err->message : "unknown error");
        g_clear_error(&err);
        exit(1);
    }
    if (size > max_size) {
        error_report("nikon-d850: flash image '%s' (%" G_GSIZE_FORMAT
                     " bytes) does not fit in the window (%" PRIu64
                     " bytes)", path, size, max_size);
        g_free(data);
        exit(1);
    }
    if (s->veneer_slot) {
        d850_apply_veneer_slot(s, (uint8_t *)data, size);
    }
    rom_add_blob_fixed(path, data, size, addr);
    g_free(data);
    s->a1010_size = (uint32_t)size;
}

static bool d850_addr_in_ram(NikonD850MachineState *s, MachineState *machine,
                             uint64_t addr, uint64_t len)
{
    if (len <= machine->ram_size && addr >= D850_RAM_BASE &&
        addr - D850_RAM_BASE <= machine->ram_size - len) {
        return true;
    }
    if (s->auxram_size >= len && addr >= D850_AUXRAM_BASE &&
        addr - D850_AUXRAM_BASE <= s->auxram_size - len) {
        return true;
    }
    return false;
}

/*
 * b1010's local init (0x9025DA40) ends with `mcr p15, 0, r2, c1, c0, 0`
 * writing SCTLR = 0x1C05, i.e. it enables the MMU.  On real hardware the
 * boot ROM has already installed TTBR0, DACR and an identity page table;
 * this prototype starts at the stub, so it has to supply that state.
 *
 * This builds the missing pieces architecturally (no CP15 field pokes):
 * a 4 GiB identity map of 1 MiB sections, plus a small ARM prologue in low
 * RAM that programs DACR/TTBR0/TTBCR and branches to the stub.
 *
 * By default the table goes at the top of DRAM and the config slot
 * 0x100F48C0 is overridden with its address.  With `mmu-table=ADDR` the
 * table is placed at ADDR instead and the config slot is left to the flash
 * image (this is how the image's own 0x201E2200 pointer is tested).
 */
static uint64_t d850_build_boot_mmu(MachineState *machine,
                                    NikonD850MachineState *s)
{
    uint64_t table_base;
    uint64_t vec = D850_LOWRAM_BASE + D850_BOOT_MMU_TRAMPOLINE;
    bool override_slot;
    uint8_t *table;
    uint32_t code[10] = {
        0xE59F0014,             /* ldr r0, [pc, #0x14]  -> 0xFFFFFFFF     */
        0xEE030F10,             /* mcr p15, 0, r0, c3, c0, 0  (DACR)      */
        0xE59F0010,             /* ldr r0, [pc, #0x10]  -> table_base     */
        0xEE020F10,             /* mcr p15, 0, r0, c2, c0, 0  (TTBR0)     */
        0xE3A00000,             /* mov r0, #0                             */
        0xEE020F50,             /* mcr p15, 0, r0, c2, c0, 2  (TTBCR)     */
        0xE59FF004,             /* ldr pc, [pc, #4]     -> stub           */
        0xFFFFFFFF,             /* DACR literal                           */
        0,                      /* TTBR0 literal, filled below            */
        0,                      /* stub entry literal, filled below       */
    };
    uint8_t blob[sizeof(code)];
    int i;

    if (s->mmu_table) {
        table_base = s->mmu_table;
        override_slot = false;
    } else {
        table_base = D850_RAM_BASE + machine->ram_size
                     - D850_BOOT_MMU_TABLE_DELTA;
        override_slot = true;
    }
    if (table_base & (D850_BOOT_MMU_TABLE_SIZE - 1)) {
        error_report("nikon-d850: mmu-table 0x%08" PRIx64
                     " is not %" PRIu64 "-byte aligned",
                     table_base, (uint64_t)D850_BOOT_MMU_TABLE_SIZE);
        exit(1);
    }
    if (!d850_addr_in_ram(s, machine, table_base,
                          D850_BOOT_MMU_TABLE_SIZE)) {
        error_report("nikon-d850: mmu-table 0x%08" PRIx64
                     " is not backed by main or aux RAM", table_base);
        exit(1);
    }

    table = g_malloc0(D850_BOOT_MMU_TABLE_SIZE);
    for (i = 0; i < 4096; i++) {
        /* section descriptor: PA | AP=rw | C=1 | B=1 | type=section */
        stl_le_p(table + i * 4, ((uint32_t)i << 20) | 0x0C0E);
    }
    rom_add_blob_fixed("d850.boot-pagetable", table,
                       D850_BOOT_MMU_TABLE_SIZE, table_base);
    g_free(table);

    /*
     * b1010's init does:
     *     r0 = *(0x100F48C0);
     *     TTBR0 = (r0 & ~0x3FF) | 0x42;
     *     TTBR1 = TTBR0;
     * so the config slot is the page-table base.  With zero flash it is 0
     * and the MMU comes up with TTBR0 = 0x42; supply our table instead.
     */
    if (override_slot) {
        uint8_t slot[4];

        stl_le_p(slot, (uint32_t)table_base);
        rom_add_blob_fixed("d850.boot-cfg0", slot, sizeof(slot),
                           D850_CONFIG_SLOT0);
    }

    code[8] = (uint32_t)table_base;
    code[9] = (uint32_t)D850_BOOT_STUB;
    for (i = 0; i < (int)ARRAY_SIZE(code); i++) {
        stl_le_p(blob + i * 4, code[i]);
    }
    rom_add_blob_fixed("d850.boot-mmu", blob, sizeof(blob), vec);

    info_report("nikon-d850: M1 boot-ROM MMU shim: identity 4 GiB, "
                "table=0x%08" PRIx64 " (config slot %s), trampoline=0x%08"
                PRIx64, table_base,
                override_slot ? "overridden" : "from image", vec);
    return vec;
}

/* ------------------------------------------------------------------ */
/* Machine                                                             */
/* ------------------------------------------------------------------ */

static void nikon_d850_init(MachineState *machine)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(machine);
    MemoryRegion *sysmem = get_system_memory();
    const char *payload = s->payload ? s->payload : machine->kernel_filename;
    const char *flash = s->flash_path ? s->flash_path : machine->firmware;
    DeviceState *mpcore;
    SysBusDevice *busdev;
    uint64_t entry;
    uint64_t start;
    int i;

    if (!payload) {
        error_report("nikon-d850: a decrypted main payload is required; "
                     "pass -kernel FILE or -machine nikon-d850,payload=FILE");
        exit(1);
    }
    if (strcmp(s->boot, "entry") != 0 && strcmp(s->boot, "stub") != 0) {
        error_report("nikon-d850: boot must be 'entry' (M0) or 'stub' (M1), "
                     "not '%s'", s->boot);
        exit(1);
    }
    if (machine->ram_size < D850_RAM_MIN ||
        machine->ram_size > D850_RAM_MAX) {
        error_report("nikon-d850: RAM size must be between %" PRIu64
                     " and %" PRIu64 " bytes",
                     (uint64_t)D850_RAM_MIN, (uint64_t)D850_RAM_MAX);
        exit(1);
    }
    if (s->flash_size < 1 * MiB || s->flash_size > D850_FLASH_SIZE_MAX ||
        (s->flash_size & 0xFFFF)) {
        error_report("nikon-d850: flash-size must be a 64 KiB multiple "
                     "between 1 MiB and %" PRIu64 " bytes",
                     (uint64_t)D850_FLASH_SIZE_MAX);
        exit(1);
    }
    if (s->auxram_size > 64 * MiB || (s->auxram_size & 0xFFFF)) {
        error_report("nikon-d850: aux-ram-size must be a 64 KiB multiple "
                     "up to 64 MiB");
        exit(1);
    }
    if ((s->aux2_size && !s->aux2_base) || s->aux2_size > 64 * MiB ||
        (s->aux2_size & 0xFFFF)) {
        error_report("nikon-d850: aux2-base and a 64 KiB-multiple aux2-size "
                     "(up to 64 MiB) are required together");
        exit(1);
    }
    if (!((s->payload_base >= D850_RAM_BASE &&
           s->payload_base < D850_RAM_BASE + machine->ram_size) ||
          (s->payload_base >= D850_FLASH_BASE &&
           s->payload_base < D850_FLASH_BASE + s->flash_size))) {
        error_report("nikon-d850: payload-base 0x%08" PRIx64
                     " is outside both the RAM and flash windows",
                     s->payload_base);
        exit(1);
    }

    /* Main DRAM window. */
    memory_region_add_subregion(sysmem, D850_RAM_BASE, machine->ram);

    /* First-stage / flash window.  RAM-backed so the prototype can load a
     * module into it; the production model should become a real ROM. */
    memory_region_init_ram(&s->flash, NULL, "nikon-d850.flash",
                           s->flash_size, &error_fatal);
    memory_region_add_subregion(sysmem, D850_FLASH_BASE, &s->flash);
    if (flash) {
        uint64_t limit = D850_FLASH_BASE + s->flash_size;

        if (s->a1010_base < D850_FLASH_BASE || s->a1010_base >= limit) {
            error_report("nikon-d850: a1010-base 0x%08" PRIx64
                         " is outside the flash window [0x%08" PRIx64
                         ", 0x%08" PRIx64 ")",
                         s->a1010_base, (uint64_t)D850_FLASH_BASE, limit);
            exit(1);
        }
        d850_load_flash(s, flash, s->a1010_base, limit - s->a1010_base);
    }

    /* Optional second RAM bank at 0x20000000 (the M-core's SRAM window and
     * the likely page-table home; see docs/m1-tracks-2026-11-09.md). */
    if (s->auxram_size) {
        memory_region_init_ram(&s->auxram, NULL, "nikon-d850.auxram",
                               s->auxram_size, &error_fatal);
        memory_region_add_subregion(sysmem, D850_AUXRAM_BASE, &s->auxram);
    }

    /* E5 boot-ROM fixture: the in-image init points TTBR1 at 0x40000000 and
     * TTBR0 at 0x40001000 and enables LPAE.  Install two 4-entry LPAE level-1
     * tables (1 GiB blocks, identity) there so the MMU-on fetch can resolve.
     * This is a fixture, not a dump of the real boot ROM: it only satisfies
     * translation, it does not provide the boot ROM's code or services. */
    if (s->e5_tables) {
        uint64_t blob[0x2000 / 8] = { 0 };
        unsigned n;

        if (s->vbr_on) {
            error_report("nikon-d850: e5-tables and virtual-boot-rom both map "
                         "0x40000000; enable only one");
            exit(1);
        }
        if (!s->aux2_size) {
            s->aux2_base = D850_HANDLER_BASE;
            s->aux2_size = D850_HANDLER_SIZE;
        }
        if (s->aux2_base != D850_HANDLER_BASE || s->aux2_size < 0x2000) {
            error_report("nikon-d850: e5-tables needs aux2-base=0x40000000 "
                         "and aux2-size >= 0x2000");
            exit(1);
        }
        for (n = 0; n < 4; n++) {
            /* LPAE level-1 block: valid, 1 GiB, AF=1, SH=inner, AP=rw EL1 */
            blob[n] = ((uint64_t)n << 30) | 0x701;
            blob[0x1000 / 8 + n] = ((uint64_t)n << 30) | 0x701;
        }
        rom_add_blob_fixed("d850.e5-lpae", blob, sizeof(blob),
                           D850_HANDLER_BASE);
        info_report("nikon-d850: e5-tables fixture installed at 0x%08" PRIx64
                    " (LPAE identity, 1 GiB blocks)",
                    (uint64_t)D850_HANDLER_BASE);
    }

    /* Optional handler RAM (E5 D850 vectors point at 0x4002xxxx). */
    if (s->aux2_size) {
        memory_region_init_ram(&s->aux2, NULL, "nikon-d850.aux2",
                               s->aux2_size, &error_fatal);
        memory_region_add_subregion(sysmem, s->aux2_base, &s->aux2);
    }

    /* D780 boot SRAM, high stacks and the 0x20022300 poll word. */
    if (s->e6_prep) {
        uint8_t ready[4];

        memory_region_init_ram(&s->e6sram, NULL, "nikon-d850.e6sram",
                               0x10000, &error_fatal);
        memory_region_add_subregion(sysmem, 0x04000000, &s->e6sram);
        memory_region_init_ram(&s->e6stack, NULL, "nikon-d850.e6stack",
                               0x4000000, &error_fatal);
        memory_region_add_subregion(sysmem, 0xC0000000, &s->e6stack);
        if (!s->auxram_size) {
            memory_region_init_ram(&s->auxram, NULL, "nikon-d850.e6ram",
                                   4 * MiB, &error_fatal);
            memory_region_add_subregion(sysmem, D850_AUXRAM_BASE,
                                        &s->auxram);
        }
        stl_le_p(ready, 1);
        rom_add_blob_fixed("d850.e6-ready", ready, sizeof(ready), 0x20022300);
        rom_add_blob_fixed("d850.e6-ready2", ready, sizeof(ready), 0x20070008);
    }

    /* EXPEED 4 register-block stubs: A2/AD/AE/A340. */
    if (s->io_stub) {
        static const struct {
            uint64_t base;
            uint32_t size;
            const char *name;
        } blocks[4] = {
            { D850_IO_A2_BASE, D850_IO_A2_SIZE, "a2" },
            { D850_IO_AD_BASE, D850_IO_AD_SIZE, "ad" },
            { D850_IO_AE_BASE, D850_IO_AE_SIZE, "ae" },
            { D850_IO_A340_BASE, D850_IO_A340_SIZE, "a340" },
        };
        int b;

        for (b = 0; b < 4; b++) {
            struct D850IoStub *st = g_new0(struct D850IoStub, 1);

            st->base = blocks[b].base;
            st->size = blocks[b].size;
            st->regs = g_new0(uint32_t, blocks[b].size / 4);
            memory_region_init_io(&st->mr, OBJECT(s), &d850_io_stub_ops, st,
                                  g_strdup_printf("nikon-d850.io-%s",
                                                  blocks[b].name),
                                  blocks[b].size);
            memory_region_add_subregion(sysmem, blocks[b].base, &st->mr);
            s->io_blocks[b] = st;
        }
    }

    /* Catch-all diagnostics.  The window is always present: strict mode
     * changes the contract from "log and continue" to "log and stop", so
     * the first missing access still leaves an address in qemu.log. */
    memory_region_init_io(&s->probe, OBJECT(s), &d850_probe_ops, s,
                          "nikon-d850.probe", D850_PROBE_SIZE);
    memory_region_add_subregion_overlap(sysmem, 0, &s->probe,
                                        D850_PROBE_PRIORITY);

    /* Virtual 0x40000000 boot ROM: map handler RAM and place "bx lr" stubs
     * at the D850/D5 exception-vector targets so the image can run without the
     * real boot ROM. */
    if (s->vbr_on) {
        static const uint32_t stubs[] = {
            0x4002DFA0, 0x4002DFC0, 0x4002E050, 0x4002E070, 0x4002E0C8,
            0x4002E0D8, 0x4002E0E8, 0x4002E0F8, 0x4002E108, 0x4002E10C,
            0x4002E12C,
        };
        static const uint8_t bx_lr[4] = { 0x1e, 0xff, 0x2f, 0xe1 };
        unsigned n;

        memory_region_init_ram(&s->vbr, NULL, "nikon-d850.vbr",
                               0x40000, &error_fatal);
        memory_region_add_subregion(sysmem, 0x40000000, &s->vbr);
        for (n = 0; n < ARRAY_SIZE(stubs); n++) {
            rom_add_blob_fixed("d850.vbr", bx_lr, sizeof(bx_lr), stubs[n]);
        }
    }

    /* Low memory: the entry stores a word at address 0 and reads it back,
     * so the first 64 KiB must be writable RAM.  A four-byte halt window
     * sits at the assisted return address: fetching it ends the run. */
    memory_region_init_ram(&s->lowram, NULL, "nikon-d850.lowram",
                           D850_LOWRAM_SIZE, &error_fatal);
    memory_region_add_subregion(sysmem, D850_LOWRAM_BASE, &s->lowram);
    memory_region_init_io(&s->halt, NULL, &d850_halt_ops, s,
                          "nikon-d850.halt", 4);
    memory_region_add_subregion_overlap(sysmem, D850_HALT_TRAMPOLINE,
                                        &s->halt, 1);

    /* CPUs.  The firmware is ARMv7-A Thumb-2 with VFP/NEON; Cortex-A9 is
     * the working hypothesis for EXPEED 4.  -cpu overrides it. */
    for (i = 0; i < machine->smp.cpus; i++) {
        Object *cpuobj = object_new(machine->cpu_type);

        qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);
    }

    /* Cortex-A9 MPCore private block (GIC + timers + watchdog).  A non-A9
     * CPU (D780/E6 uses Cortex-A15-class cores with UDIV) gets a generic
     * GIC at the same distributor/CPU-interface offsets instead. */
    if (!strcmp(machine->cpu_type, ARM_CPU_TYPE_NAME("cortex-a9"))) {
        mpcore = qdev_new(TYPE_A9MPCORE_PRIV);
        qdev_prop_set_uint32(mpcore, "num-cpu", machine->smp.cpus);
        qdev_prop_set_uint32(mpcore, "num-irq", D850_GIC_NUM_IRQ);
        busdev = SYS_BUS_DEVICE(mpcore);
        sysbus_realize_and_unref(busdev, &error_fatal);
        sysbus_mmio_map(busdev, 0, s->mpcore_base);
        for (i = 0; i < machine->smp.cpus; i++) {
            DeviceState *cpudev = DEVICE(qemu_get_cpu(i));

            sysbus_connect_irq(busdev, i,
                               qdev_get_gpio_in(cpudev, D850_CPU_IRQ));
            sysbus_connect_irq(busdev, i + machine->smp.cpus,
                               qdev_get_gpio_in(cpudev, D850_CPU_FIQ));
            sysbus_connect_irq(busdev, i + 2 * machine->smp.cpus,
                               qdev_get_gpio_in(cpudev, D850_CPU_VIRQ));
            sysbus_connect_irq(busdev, i + 3 * machine->smp.cpus,
                               qdev_get_gpio_in(cpudev, D850_CPU_VFIQ));
        }
    } else {
        DeviceState *gic = qdev_new(TYPE_ARM_GIC);

        qdev_prop_set_uint32(gic, "num-cpu", machine->smp.cpus);
        qdev_prop_set_uint32(gic, "num-irq", D850_GIC_NUM_IRQ);
        busdev = SYS_BUS_DEVICE(gic);
        sysbus_realize_and_unref(busdev, &error_fatal);
        sysbus_mmio_map(busdev, 0, s->mpcore_base + 0x1000);
        sysbus_mmio_map(busdev, 1, s->mpcore_base + 0x2000);
        for (i = 0; i < machine->smp.cpus; i++) {
            DeviceState *cpudev = DEVICE(qemu_get_cpu(i));

            sysbus_connect_irq(busdev, i,
                               qdev_get_gpio_in(cpudev, D850_CPU_IRQ));
            sysbus_connect_irq(busdev, i + machine->smp.cpus,
                               qdev_get_gpio_in(cpudev, D850_CPU_FIQ));
        }
    }

    /* Candidate serial block. */
    memory_region_init_io(&s->uart, OBJECT(s), &d850_uart_ops, s,
                          "nikon-d850.uart", D850_UART_SIZE);
    memory_region_add_subregion(sysmem, D850_UART_BASE, &s->uart);
    if (serial_hd(0)) {
        qemu_chr_fe_init(&s->chr, serial_hd(0), &error_abort);
    }

    /* Payload goes in last, after the RAM window exists. */
    d850_load_payload(s, payload, s->payload_base,
                      s->payload_base >= D850_RAM_BASE
                      ? machine->ram_size - (s->payload_base - D850_RAM_BASE)
                      : s->flash_size - (s->payload_base - D850_FLASH_BASE));

    entry = s->entry == UINT64_MAX ? D850_ENTRY_ADDR : s->entry;
    if (strcmp(s->boot, "stub") != 0) {
        start = entry;
    } else if (s->mmu_bringup) {
        start = d850_build_boot_mmu(machine, s);
    } else {
        start = D850_BOOT_STUB;
    }

    for (i = 0; i < machine->smp.cpus; i++) {
        CPUState *cs = qemu_get_cpu(i);
        ARMCPU *cpu = ARM_CPU(cs);

        if (!strcmp(s->boot, "stub")) {
            /* M1: start where the real chain starts and let b1010 hand
             * control to a1010.  SVC mode with IRQ/FIQ masked, a stack near
             * the top of DRAM, LR=0 so an unexpected return is visible. */
            cpu->env.uncached_cpsr = ARM_CPU_MODE_SVC | CPSR_I | CPSR_F;
            cpu->env.thumb = 0;
            cpu->env.regs[13] = D850_RAM_BASE + machine->ram_size
                                - D850_BOOT_STUB_SP_DELTA;
            cpu->env.regs[14] = 0;
            memset(&cpu->env.regs[0], 0,
                   sizeof(cpu->env.regs[0]) * 13);
            cpu_set_pc(cs, start);
        } else {
            /* Assisted startup: D850's reset word is a boot-service sled,
             * not a function prologue.  Supply the SVC state and a stack
             * the real boot ROM would have provided; LR points at the halt
             * window so a stray `pop {..., pc}` is visible. */
            cpu->env.uncached_cpsr = ARM_CPU_MODE_SVC | CPSR_I | CPSR_F;
            cpu->env.thumb = 0;
            cpu->env.regs[13] = D850_RAM_BASE + machine->ram_size - 0x100;
            cpu->env.regs[14] = D850_HALT_TRAMPOLINE;
            cpu->env.regs[0] = 0;
            cpu_set_pc(cs, entry);
        }
    }

    if (s->lcd_base) {
        s->lcd_timer = timer_new_ms(QEMU_CLOCK_REALTIME, d850_lcd_tick, s);
        timer_mod(s->lcd_timer,
                  qemu_clock_get_ms(QEMU_CLOCK_REALTIME) + 2000);
    }

    info_report("nikon-d850: boot=%s, payload=%s @ 0x%08" PRIx64
                ", flash=%s"
                ", start=0x%08" PRIx64 ", ram=%" PRIu64 " MiB, cpus=%u"
                "%s%s%s%s%s%s%s%s%s%s",
                s->boot, payload, s->payload_base,
                flash ? flash : "(none)",
                start,
                machine->ram_size / MiB, machine->smp.cpus,
                s->strict ? ", strict" : "",
                s->thumb_fix ? ", thumb-fix" : "",
                s->vector_fix ? ", vector-fix" : "",
                s->veneer_slot ? ", veneer-slot" : "",
                s->loader_copies ? ", loader-copies" : "",
                s->io_stub ? ", io-stub" : "",
                s->flash_size != D850_FLASH_SIZE ? ", custom-flash" : "",
                s->auxram_size ? ", aux-ram" : "",
                s->e5_tables ? ", e5-tables" : "",
                s->mmu_table ? ", mmu-table" : "");
}

static void nikon_d850_instance_init(Object *obj)
{
    NikonD850MachineState *s = NIKON_D850_MACHINE(obj);

    s->entry = UINT64_MAX;
    s->payload_base = D850_PAYLOAD_BASE;
    s->boot = g_strdup("entry");
    s->a1010_base = D850_A1010_BASE;
    s->flash_size = D850_FLASH_SIZE;
    s->thumb_fix = false;
    s->vector_fix = false;
    s->veneer_slot = 0;
    s->loader_copies = false;
    s->io_stub = false;
    s->e6_prep = false;
    s->vbr_on = false;
    s->e5_tables = false;
    s->mmu_bringup = false;
    s->mmu_table = 0;
    s->auxram_size = 0;
    s->aux2_base = 0;
    s->aux2_size = 0;
    s->mpcore_base = D850_MPCORE_BASE;
    s->uart_status = 0x03;
    s->lcd_width = 640;
    s->lcd_height = 480;
    s->lcd_format = g_strdup("argb8888");
    s->lcd_out = g_strdup("lcd.ppm");
}

static void nikon_d850_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static const char *const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-a9"),
        ARM_CPU_TYPE_NAME("cortex-a7"),
        ARM_CPU_TYPE_NAME("cortex-a15"),
        NULL
    };

    mc->desc = "Nikon D850 validation prototype (EXPEED 5 / E5, boot-ROM experiments)";
    mc->init = nikon_d850_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a15");
    mc->valid_cpu_types = valid_cpu_types;
    mc->max_cpus = 2;
    mc->min_cpus = 1;
    mc->default_ram_size = 128 * MiB;
    mc->default_ram_id = "nikon-d850.ram";

    object_class_property_add_str(oc, "payload",
                                  nikon_d850_get_payload,
                                  nikon_d850_set_payload);
    object_class_property_set_description(oc, "payload",
        "Path to the decrypted main payload (normally b1330_<ver>.bin)");
    object_class_property_add_str(oc, "flash",
                                  nikon_d850_get_flash,
                                  nikon_d850_set_flash);
    object_class_property_set_description(oc, "flash",
        "Path to an optional second flash image (legacy cross-model runs; "
        "the D850 profile loads everything with 'payload')");
    object_class_property_add(oc, "entry", "uint64",
                              nikon_d850_get_entry, nikon_d850_set_entry,
                              NULL, NULL);
    object_class_property_set_description(oc, "entry",
        "Initial PC; default is the D850 reset word target (0x1037CBA8)");
    object_class_property_add(oc, "payload-base", "uint64",
                              nikon_d850_get_payload_base,
                              nikon_d850_set_payload_base, NULL, NULL);
    object_class_property_set_description(oc, "payload-base",
        "Address the b1330 image is loaded at (default 0x10000000, the "
        "D850 flash window)");
    object_class_property_add_bool(oc, "strict",
                                   nikon_d850_get_strict,
                                   nikon_d850_set_strict);
    object_class_property_set_description(oc, "strict",
        "Stop the run at the first unmodelled MMIO access (logs d850-strict:)");

    object_class_property_add_str(oc, "boot-mode",
                                  nikon_d850_get_boot,
                                  nikon_d850_set_boot);
    object_class_property_set_description(oc, "boot-mode",
        "Startup mode: 'entry' (default, assisted D850 start at --entry) or "
        "'stub' (legacy EXPEED-4 boot chain)");
    object_class_property_add(oc, "a1010-base", "uint64",
                              nikon_d850_get_a1010_base,
                              nikon_d850_set_a1010_base, NULL, NULL);
    object_class_property_set_description(oc, "a1010-base",
        "Address where the a1010 module is mapped inside the flash window (default 0x10018000)");
    object_class_property_add_bool(oc, "thumb-fix",
                                   nikon_d850_get_thumb_fix,
                                   nikon_d850_set_thumb_fix);
    object_class_property_set_description(oc, "thumb-fix",
        "Patch the in-memory bx literal to 0x1002CB8D so a1010 is entered in Thumb state");
    object_class_property_add_bool(oc, "mmu-bringup",
                                   nikon_d850_get_mmu_bringup,
                                   nikon_d850_set_mmu_bringup);
    object_class_property_set_description(oc, "mmu-bringup",
        "Supply the boot ROM's identity page table and TTBR0/DACR before the "
        "stub enables the MMU (default on; use off only to experiment)");
    object_class_property_add(oc, "mpcore-base", "uint64",
                              nikon_d850_get_mpcore_base,
                              nikon_d850_set_mpcore_base, NULL, NULL);
    object_class_property_set_description(oc, "mpcore-base",
        "Cortex-A9 MPCore (GIC/timers/watchdog) base; default 0xE000E000, "
        "inherited from the EXPEED-4 prototype until D850 makes its own "
        "access");
    object_class_property_add(oc, "flash-size", "uint64",
                              nikon_d850_get_flash_size,
                              nikon_d850_set_flash_size, NULL, NULL);
    object_class_property_set_description(oc, "flash-size",
        "Flash window size (default 32 MiB, enough for the 25.7 MB D850 "
        "b1330 image; up to 64 MiB)");
    object_class_property_add(oc, "aux-ram-size", "uint64",
                              nikon_d850_get_auxram_size,
                              nikon_d850_set_auxram_size, NULL, NULL);
    object_class_property_set_description(oc, "aux-ram-size",
        "Optional RAM bank at 0x20000000 (0 = disabled); e.g. 4M for the "
        "0x201E2000 page-table experiment");
    object_class_property_add(oc, "aux2-base", "uint64",
                              nikon_d850_get_aux2_base,
                              nikon_d850_set_aux2_base, NULL, NULL);
    object_class_property_set_description(oc, "aux2-base",
        "Optional handler-RAM base (E5 D850 vectors point at 0x40000000)");
    object_class_property_add(oc, "aux2-size", "uint64",
                              nikon_d850_get_aux2_size,
                              nikon_d850_set_aux2_size, NULL, NULL);
    object_class_property_set_description(oc, "aux2-size",
        "Optional handler-RAM size (64 KiB multiple, up to 64 MiB)");
    object_class_property_add(oc, "mmu-table", "uint64",
                              nikon_d850_get_mmu_table,
                              nikon_d850_set_mmu_table, NULL, NULL);
    object_class_property_set_description(oc, "mmu-table",
        "Place the identity table at this address and leave config slot "
        "0x100F48C0 to the flash image (0 = auto at top of DRAM)");
    object_class_property_add_bool(oc, "vector-fix",
                                   nikon_d850_get_vector_fix,
                                   nikon_d850_set_vector_fix);
    object_class_property_set_description(oc, "vector-fix",
        "Install a minimal ARM vector table at 0x90020000 (SVC returns "
        "with the Thumb bit restored, IRQ/FIQ return, faults spin)");
    object_class_property_add(oc, "veneer-slot", "uint64",
                              nikon_d850_get_veneer_slot,
                              nikon_d850_set_veneer_slot, NULL, NULL);
    object_class_property_set_description(oc, "veneer-slot",
        "Payload offset of one ldr-pc veneer target to mark as Thumb (0 = off)");
    object_class_property_add_bool(oc, "loader-copies",
                                   nikon_d850_get_loader_copies,
                                   nikon_d850_set_loader_copies);
    object_class_property_set_description(oc, "loader-copies",
        "Apply the four descriptor-table (dest,src,size) flash-to-RAM copies "
        "to the in-memory payload before boot");
    object_class_property_add_bool(oc, "io-stub",
                                   nikon_d850_get_io_stub,
                                   nikon_d850_set_io_stub);
    object_class_property_set_description(oc, "io-stub",
        "Map the A2/AD/AE/A340 register blocks as a register file and report "
        "0xAE00060C as ready (diagnostic)");
    object_class_property_add_bool(oc, "e6-prep",
                                   nikon_d850_get_e6_prep,
                                   nikon_d850_set_e6_prep);
    object_class_property_set_description(oc, "e6-prep",
        "D780 prep: turn the page table at payload+0x10000 into identity "
        "sections and map boot SRAM 0x04000000 plus stacks at 0xC0000000");
    object_class_property_add_bool(oc, "virtual-boot-rom",
                                   nikon_d850_get_vbr,
                                   nikon_d850_set_vbr);
    object_class_property_set_description(oc, "virtual-boot-rom",
        "Map 0x40000000 as RAM and place bx lr stubs at the E5 handler "
        "addresses (diagnostic; the real boot ROM contents are unknown)");
    object_class_property_add_bool(oc, "e5-tables",
                                   nikon_d850_get_e5_tables,
                                   nikon_d850_set_e5_tables);
    object_class_property_set_description(oc, "e5-tables",
        "Map 0x40000000 as handler RAM and install LPAE identity tables at "
        "0x40000000/0x40001000 (fixture for the missing E5 boot ROM tables)");

    object_class_property_add(oc, "lcd-base", "uint64",
                              nikon_d850_get_lcd_base,
                              nikon_d850_set_lcd_base, NULL, NULL);
    object_class_property_set_description(oc, "lcd-base",
        "Guest framebuffer base for the optional PPM dump (0 = disabled)");
    object_class_property_add(oc, "lcd-width", "uint32",
                              nikon_d850_get_lcd_width,
                              nikon_d850_set_lcd_width, NULL, NULL);
    object_class_property_set_description(oc, "lcd-width",
        "Framebuffer width in pixels (default 640)");
    object_class_property_add(oc, "lcd-height", "uint32",
                              nikon_d850_get_lcd_height,
                              nikon_d850_set_lcd_height, NULL, NULL);
    object_class_property_set_description(oc, "lcd-height",
        "Framebuffer height in pixels (default 480)");
    object_class_property_add(oc, "lcd-stride", "uint32",
                              nikon_d850_get_lcd_stride,
                              nikon_d850_set_lcd_stride, NULL, NULL);
    object_class_property_set_description(oc, "lcd-stride",
        "Framebuffer stride in bytes (0 = width * bytes-per-pixel)");
    object_class_property_add_str(oc, "lcd-format",
                                  nikon_d850_get_lcd_format,
                                  nikon_d850_set_lcd_format);
    object_class_property_set_description(oc, "lcd-format",
        "argb8888 (default), rgb888 or rgb565");
    object_class_property_add_str(oc, "lcd-out",
                                  nikon_d850_get_lcd_out,
                                  nikon_d850_set_lcd_out);
    object_class_property_set_description(oc, "lcd-out",
        "PPM output path (default lcd.ppm)");
}

static const TypeInfo nikon_d850_info = {
    .name = TYPE_NIKON_D850_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(NikonD850MachineState),
    .instance_init = nikon_d850_instance_init,
    .class_init = nikon_d850_class_init,
    .interfaces = arm_machine_interfaces,
};

static void nikon_d850_register_types(void)
{
    type_register_static(&nikon_d850_info);
}

type_init(nikon_d850_register_types);

/* ================================================================== */
/* Nikon D850 M-core (Firmware A / a1330) validation prototype        */
/* ================================================================== */
/*
 * The D850 carries a second, Cortex-M class core whose firmware is the
 * "Firmware A" image (a1330, 1 MiB, BE8: little-endian Thumb code with a
 * big-endian vector table).  This machine is deliberately minimal -- 1 MiB
 * flash at 0, 256 KiB SRAM at 0x20000000, and a catch-all diagnostic window.
 * It exists to answer one question at a time: which peripheral does the
 * M-core touch first, and how far does its reset path get before a modelled
 * device is required?
 *
 * The image is a local, user-supplied input; nothing is embedded here.
 */

#define TYPE_NIKON_D850_MCORE_MACHINE MACHINE_TYPE_NAME("nikon-d850-mcore")

#define MCORE_FLASH_SIZE (1 * MiB)
#define MCORE_SRAM_BASE  0x20000000ULL
#define MCORE_SRAM_SIZE  (256 * KiB)

typedef struct NikonD850McoreState {
    MachineState parent;
    MemoryRegion board;         /* passed to armv7m as the physical space */
    MemoryRegion flash;         /* a1330 at 0x00000000                    */
    MemoryRegion sram;          /* MSP lives near the top of this bank    */
    MemoryRegion probe;         /* catch-all diagnostic window            */
    ARMv7MState armv7m;
    MemoryRegion scb;           /* BE8-visible CPACR/VTOR shim            */
    uint32_t scb_regs[0x100 / 4];
    uint8_t *image;
    uint8_t *flash_image;
    gsize image_size;
    bool strict;
    uint64_t probe_events;
} NikonD850McoreState;

DECLARE_INSTANCE_CHECKER(NikonD850McoreState, NIKON_D850_MCORE,
                         TYPE_NIKON_D850_MCORE_MACHINE)

static uint32_t mcore_guest_pc(void)
{
    CPUState *cs = current_cpu;

    return cs ? ARM_CPU(cs)->env.regs[15] : 0;
}

static uint64_t mcore_probe_read(void *opaque, hwaddr addr, unsigned size)
{
    NikonD850McoreState *s = opaque;

    if (s->strict) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "mcore-strict: pc=0x%08x rd addr=0x%08" HWADDR_PRIx
                      " size=%u (first unmodelled access, stopping)\n",
                      mcore_guest_pc(), addr, size);
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
        return 0;
    }

    if (s->probe_events < 256 || (s->probe_events % 1024) == 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "mcore-probe: pc=0x%08x rd addr=0x%08" HWADDR_PRIx
                      " size=%u (unmodelled read, returning 0)\n",
                      mcore_guest_pc(), addr, size);
    }
    s->probe_events++;
    return 0;
}

static void mcore_probe_write(void *opaque, hwaddr addr, uint64_t value,
                              unsigned size)
{
    NikonD850McoreState *s = opaque;

    if (s->strict) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "mcore-strict: pc=0x%08x wr addr=0x%08" HWADDR_PRIx
                      " size=%u value=0x%08" PRIx64
                      " (first unmodelled access, stopping)\n",
                      mcore_guest_pc(), addr, size, value);
        qemu_system_shutdown_request(SHUTDOWN_CAUSE_GUEST_SHUTDOWN);
        return;
    }

    if (s->probe_events < 256 || (s->probe_events % 1024) == 0) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "mcore-probe: pc=0x%08x wr addr=0x%08" HWADDR_PRIx
                      " size=%u value=0x%08" PRIx64
                      " (unmodelled write)\n",
                      mcore_guest_pc(), addr, size, value);
    }
    s->probe_events++;
}

static const MemoryRegionOps mcore_probe_ops = {
    .read = mcore_probe_read,
    .write = mcore_probe_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .valid.min_access_size = 1,
    .valid.max_access_size = 8,
    .impl.min_access_size = 1,
    .impl.max_access_size = 8,
};

/*
 * Diagnostic register file for the first M-core peripherals seen in the
 * reset path: 0x400F2000 (clock/reset-ish) and 0x400F3000 (command/status,
 * polled).  Reads return the last written value, which is enough to satisfy
 * the write/read-back handshakes in the reset code; it is not a model of the
 * hardware.  Reuses the A-core D850IoStub register-file ops.
 */
static void mcore_add_regfile(NikonD850McoreState *s, uint64_t base,
                              uint32_t size, const char *name)
{
    D850IoStub *st = g_new0(D850IoStub, 1);

    st->base = base;
    st->size = size;
    st->regs = g_new0(uint32_t, size / 4);
    memory_region_init_io(&st->mr, OBJECT(s), &d850_io_stub_ops, st, name,
                          size);
    memory_region_add_subregion(&s->board, base, &st->mr);
}

/*
 * The ARMv7-M NVIC models its system registers as little-endian.  A BE8
 * guest writes 0x2000FC00 to VTOR and 0x00F00000 to CPACR; through the LE
 * NVIC those arrive as 0x00FC0020 and 0x0000F000, so the vector base and the
 * FPU enable are wrong.  This tiny big-endian window shadows just the SCB
 * page and keeps VTOR/CPACR in CPUARMState, which is what the CPU actually
 * uses.  It is a BE8 fixup, not a hardware model.
 */
static uint64_t mcore_scb_read(void *opaque, hwaddr addr, unsigned size)
{
    NikonD850McoreState *s = opaque;
    CPUARMState *env = &s->armv7m.cpu->env;

    switch (addr) {
    case 0x08:
        return env->v7m.vecbase[M_REG_NS];
    case 0x88:
        return env->v7m.cpacr[M_REG_NS];
    default:
        return (addr < sizeof(s->scb_regs)) ? s->scb_regs[addr / 4] : 0;
    }
}

static void mcore_scb_write(void *opaque, hwaddr addr, uint64_t value,
                            unsigned size)
{
    NikonD850McoreState *s = opaque;
    CPUARMState *env = &s->armv7m.cpu->env;

    switch (addr) {
    case 0x08:
        env->v7m.vecbase[M_REG_NS] = value & 0xffffff80u;
        break;
    case 0x88:
        env->v7m.cpacr[M_REG_NS] = value & (0xfu << 20);
        break;
    default:
        if (addr < sizeof(s->scb_regs)) {
            s->scb_regs[addr / 4] = value;
        }
        break;
    }
}

static const MemoryRegionOps mcore_scb_ops = {
    .read = mcore_scb_read,
    .write = mcore_scb_write,
    .endianness = DEVICE_BIG_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .impl.min_access_size = 4,
    .impl.max_access_size = 4,
};

/*
 * The a1330 vector table is stored big-endian while the Thumb code is
 * little-endian (BE8).  QEMU's M-profile reset reads the table with ldl_p
 * (little-endian), so it gets 0x30fa0020/0x0dc20000 instead of the real
 * MSP/PC.  We therefore take SP/PC from the raw file and force CPSR.E so
 * data accesses are big-endian while instruction fetch stays little-endian.
 * The ROM blob is copied into place during the same reset; the reset
 * handler only has to fix up the CPU state.
 */
static void nikon_d850_mcore_reset(void *opaque)
{
    NikonD850McoreState *s = opaque;
    ARMCPU *cpu;
    CPUARMState *env;
    uint32_t sp, pc;

    if (!s->armv7m.cpu || s->image_size < 8) {
        return;
    }
    cpu = s->armv7m.cpu;
    env = &cpu->env;

    sp = ldl_be_p(s->image);
    pc = ldl_be_p(s->image + 4);

    env->regs[13] = sp & ~3u;
    env->regs[15] = pc & ~1u;
    env->thumb = 1;
    env->regs[14] = 0xffffffff;
    env->v7m.exception = 0;
    env->v7m.vecbase[M_REG_NS] = 0;
    env->v7m.control[M_REG_NS] = 0;
    env->uncached_cpsr |= CPSR_E;
    arm_rebuild_hflags(env);
}

static void nikon_d850_mcore_init(MachineState *machine)
{
    NikonD850McoreState *s = NIKON_D850_MCORE(machine);
    DeviceState *dev;
    Clock *clk;
    gsize size = 0;
    GError *err = NULL;
    unsigned i;

    if (!machine->kernel_filename) {
        error_report("nikon-d850-mcore: -kernel FILE (a1330 Firmware A) is "
                     "required");
        exit(1);
    }
    if (!g_file_get_contents(machine->kernel_filename, (gchar **)&s->image,
                             &size, &err)) {
        error_report("%s", err->message);
        g_error_free(err);
        exit(1);
    }
    if (size < 8 || size > MCORE_FLASH_SIZE) {
        error_report("nikon-d850-mcore: image must be 8..%" PRIu64 " bytes",
                     (uint64_t)MCORE_FLASH_SIZE);
        exit(1);
    }
    s->image_size = size;

    /*
     * QEMU's M-profile reset and exception entry read the vector table with
     * ldl_p (little-endian).  The a1330 table is big-endian, so put a
     * word-swapped copy of the 16-entry table in flash for the CPU, while
     * keeping the original big-endian image for SP/PC values.  This is a
     * fixture for QEMU's LE vector fetch, not a modification of the dump.
     */
    s->flash_image = g_memdup2(s->image, s->image_size);
    for (i = 0; i < 0x40 && i + 4 <= s->image_size; i += 4) {
        stl_le_p(s->flash_image + i, ldl_be_p(s->image + i));
    }

    memory_region_init(&s->board, OBJECT(machine), "mcore.board",
                       0x100000000ULL);
    memory_region_init_ram(&s->flash, NULL, "mcore.flash",
                           MCORE_FLASH_SIZE, &error_fatal);
    memory_region_init_ram(&s->sram, NULL, "mcore.sram",
                           MCORE_SRAM_SIZE, &error_fatal);
    memcpy(memory_region_get_ram_ptr(&s->flash), s->flash_image,
           s->image_size);
    memory_region_add_subregion(&s->board, 0, &s->flash);
    memory_region_add_subregion(&s->board, MCORE_SRAM_BASE, &s->sram);

    /* The M-core has no MMU; anything not modelled must leave an address
     * and PC rather than a plausible-looking zero. */
    memory_region_init_io(&s->probe, OBJECT(machine), &mcore_probe_ops, s,
                          "mcore.probe", 0x100000000ULL);
    memory_region_add_subregion_overlap(&s->board, 0, &s->probe, -1000);

    /* First two peripherals touched by the a1330 reset path. */
    mcore_add_regfile(s, 0x400F2000, 0x1000, "mcore.clk");
    mcore_add_regfile(s, 0x400F3000, 0x1000, "mcore.cmd");

    object_initialize_child(OBJECT(machine), "armv7m", &s->armv7m,
                            TYPE_ARMV7M);
    dev = DEVICE(&s->armv7m);
    if (!machine->cpu_type) {
        machine->cpu_type = ARM_CPU_TYPE_NAME("cortex-m4");
    }
    qdev_prop_set_string(dev, "cpu-type", machine->cpu_type);
    qdev_prop_set_uint32(dev, "num-irq", 96);
    qdev_prop_set_bit(dev, "enable-bitband", true);
    qdev_prop_set_bit(dev, "vfp", true);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(&s->board), &error_fatal);

    clk = clock_new(OBJECT(machine), "mcore.cpuclk");
    clock_set_hz(clk, 100000000);
    qdev_connect_clock_in(dev, "cpuclk", clk);

    /*
     * Reset-handler ordering in QEMU's resettable container is not stable
     * across versions.  Register the state fixup on both sides of the CPU
     * realize so whichever pass runs last has the final say; the call is
     * idempotent and no-ops until armv7m.cpu exists.
     */
    qemu_register_reset(nikon_d850_mcore_reset, s);
    sysbus_realize(SYS_BUS_DEVICE(dev), &error_fatal);

    /* BE8 fixup: shadow the LE NVIC's VTOR/CPACR with a big-endian window. */
    memory_region_init_io(&s->scb, OBJECT(machine), &mcore_scb_ops, s,
                          "mcore.scb", 0x100);
    memory_region_add_subregion_overlap(&s->armv7m.container, 0xE000ED00,
                                        &s->scb, 1);

    qemu_register_reset(nikon_d850_mcore_reset, s);
    nikon_d850_mcore_reset(s);
}

static bool nikon_d850_mcore_get_strict(Object *obj, Error **errp)
{
    return NIKON_D850_MCORE(obj)->strict;
}

static void nikon_d850_mcore_set_strict(Object *obj, bool value, Error **errp)
{
    NIKON_D850_MCORE(obj)->strict = value;
}

static void nikon_d850_mcore_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Nikon D850 M-core (Firmware A / a1330) validation prototype";
    mc->init = nikon_d850_mcore_init;
    mc->max_cpus = 1;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m4");

    object_class_property_add_bool(oc, "strict",
                                   nikon_d850_mcore_get_strict,
                                   nikon_d850_mcore_set_strict);
    object_class_property_set_description(oc, "strict",
        "Stop on the first unmodelled access (the access is still logged)");
}

static const TypeInfo nikon_d850_mcore_info = {
    .name = TYPE_NIKON_D850_MCORE_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(NikonD850McoreState),
    .class_init = nikon_d850_mcore_class_init,
    .interfaces = arm_machine_interfaces,
};

static void nikon_d850_mcore_register_types(void)
{
    type_register_static(&nikon_d850_mcore_info);
}

type_init(nikon_d850_mcore_register_types);
