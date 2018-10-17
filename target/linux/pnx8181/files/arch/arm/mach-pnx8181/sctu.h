#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/io.h>

#include <asm/types.h>
/*
 * Both SCTUs share the same programming interface
 * we honor but putting the private stuff into this
 * struct and use single handler functions
 */
struct sctu_data {
	u16 *cr; /* Control */
	u16 *rr; /* Reload */
	u16 *wr; /* Work */
	u16 *c0; /* Channel 0 */
	/* More channels to follow, but we use only one */
	u8 *sr; /* Status */
	u8 *pr; /* Pre-Scaler */

	u32 freq;
	u64 cnt;
	u64 last;
	u8 scaling_factor;
	u32 ticks_per_jiffy;
	/* Use with container_of later */
	struct clock_event_device clkevt;
};

/* Helper macro to build the pointers */
#define SCTU_ADD_PTRS(REG_BASE) \
	.cr = (void *) REG_BASE + 0x00,\
	.rr = (void *) REG_BASE + 0x04,\
	.wr = (void *) REG_BASE + 0x08,\
	.c0 = (void *) REG_BASE + 0x0C,\
	.sr = (void *) REG_BASE + 0x1C,\
	.pr = (void *) REG_BASE + 0x20,


/* Handle the clock event mode */
irqreturn_t pnx8181_sctu_interrupt(int irq, void *dev_id);
cycle_t sctu_read_clk(struct sctu_data *sctu);
cycle_t sctu_read_clk_alt(struct sctu_data *sctu);
int sctu_clk_init(struct sctu_data *sctu, struct clocksource *sctu_clk,
                  struct irqaction *sctu_irq);
int sctu_clk_init_alt(struct sctu_data *sctu, struct clocksource *sctu_clk,
                  struct irqaction *sctu_irq);

