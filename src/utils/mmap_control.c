#include "mmap_control.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "logger.h"

/* PWMSS2 physical base address and size */
#define PWMSS2_BASE_PHYS (0x48304000U)
#define PWMSS_PAGE_SIZE (4096U)

/* PWMSS2 register offsets */
#define PWMSS2_CLKCONFIG_OFF (0x08U) /* pg 2332 in AM335x manual */
#define PWMSS2_EPWMCLK_EN_BIT (8U)   /* ePWMCLK_EN field is bit 8 */
#define EPWM_MODULE_OFFSET (0x200U)

/* TBCTL field masks and values */
#define TBCTL_CTRMODE_MASK ((uint16_t)0x0003U)
#define TBCTL_CTRMODE_UP ((uint16_t)0x0000U)   /* up-count mode */
#define TBCTL_CTRMODE_STOP ((uint16_t)0x0003U) /* freeze/stop   */
#define TBCTL_CLKDIV_SHIFT (10U)
#define TBCTL_HSPCLKDIV_SHIFT (7U)
#define TBCTL_FREE_SOFT_FREE ((uint16_t)0xC000U) /* FREE_SOFT [15:14] = 11 (free-run) */

/*
 * Fixed prescaler: HSPCLKDIV = ÷1 (encoding 0), CLKDIV = ÷32 (encoding 5).
 *   TBCLK = 100 MHz / (1 × 32) = 3.125 MHz → 320 ns per tick.
 */
#define TBCLK_HSPCLKDIV_ENC (0U)
#define TBCLK_CLKDIV_ENC (5U)
#define TBCLK_NS_PER_TICK (320U)

/* TBCTL value written once during init: stopped, fixed prescaler, free-run */
#define TBCTL_INIT                                                                                \
	((uint16_t)(TBCTL_FREE_SOFT_FREE | (uint16_t)(TBCLK_HSPCLKDIV_ENC << TBCTL_HSPCLKDIV_SHIFT) | \
	    (uint16_t)(TBCLK_CLKDIV_ENC << TBCTL_CLKDIV_SHIFT) | TBCTL_CTRMODE_STOP))

/*
 * Action-qualifier values for active-high PWM in up-count mode:
 *   ZRO [1:0] = 10  → set output high at CTR = 0
 *   CBU [9:8] = 01  → clear output low at CTR = CMPB (counting up) — channel B
 */
#define AQCTLA_UPCOUNT_ACTIVE_HIGH ((uint16_t)0x0012U)
#define AQCTLB_UPCOUNT_ACTIVE_HIGH ((uint16_t)0x0102U)

/* AQCSFRC continuous software force — CSFA [1:0], CSFB [3:2]; 01 = force low */
#define AQCSFRC_CSFA_MASK ((uint16_t)0x0003U)
#define AQCSFRC_CSFB_MASK ((uint16_t)0x000CU)
#define AQCSFRC_CSFA_LOW ((uint16_t)0x0001U)
#define AQCSFRC_CSFB_LOW ((uint16_t)0x0004U)

/* ePWM register offsets (from epwm_regs base at PWMSS_BASE + 0x200) */
#define EPWM_TBCTL_OFF (0x00U)
#define EPWM_TBPRD_OFF (0x0AU)
#define EPWM_CMPA_OFF (0x12U)
#define EPWM_CMPB_OFF (0x14U)
#define EPWM_AQCTLA_OFF (0x16U)
#define EPWM_AQCTLB_OFF (0x18U)
#define EPWM_AQCSFRC_OFF (0x1CU)

/* ePWM handle — definition is internal; callers use the opaque pointer */
struct epwm_mmap_handle {
	int32_t fd;
	volatile uint8_t *map_base;   /* The actual mmap'd page (for munmap) */
	volatile uint8_t *pwmss_regs; /* PWMSS_BASE (within map_base) */
	volatile uint8_t *epwm_regs;  /* PWMSS_BASE + 0x200 — ePWM registers */
	epwm_channel_t channel;
	uint32_t period_ns; /* cached for duty-cycle clamping */
};

/* 16-bit and 32-bit register accessors */
static inline volatile uint16_t *reg16(volatile uint8_t *base, uint32_t off) {
	return (volatile uint16_t *)((uintptr_t)base + off);
}

static inline volatile uint32_t *reg32(volatile uint8_t *base, uint32_t off) {
	return (volatile uint32_t *)((uintptr_t)base + off);
}

/*--------------------------------------
 * Function: epwm_mmap_init
 *
 * Opens /dev/mem and mmaps the PWMSS page. The caller MUST ensure
 * the PWMSS module clock is already enabled (e.g. via sysfs export)
 * before calling this function, otherwise register access will SIGBUS.
 *--------------------------------------*/
epwm_mmap_handle_t *epwm_mmap_init(uint8_t pwmss_instance, epwm_channel_t channel) {
	if (pwmss_instance > 2U) {
		LOG_AND_EXIT("Invalid PWMSS instance %u (valid: 0, 1, 2)", pwmss_instance);
		return NULL;
	}
	if (pwmss_instance != 2U) {
		LOG_AND_EXIT("Only PWMSS2 is currently supported (pwmss_instance=2)");
		return NULL;
	}

	uint32_t page_base = PWMSS2_BASE_PHYS & ~(PWMSS_PAGE_SIZE - 1U);
	uint32_t page_offset = PWMSS2_BASE_PHYS - page_base;

	int32_t fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		LOG_AND_EXIT("Failed to open /dev/mem (errno: %d)", errno);
		return NULL;
	}

	volatile uint8_t *map_base =
	    (volatile uint8_t *)mmap(NULL, PWMSS_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)page_base);
	if (map_base == MAP_FAILED) {
		close(fd);
		LOG_AND_EXIT("Failed to mmap PWMSS2 at 0x%08X (errno: %d)", PWMSS2_BASE_PHYS, errno);
		return NULL;
	}

	epwm_mmap_handle_t *handle = malloc(sizeof(*handle));
	if (handle == NULL) {
		munmap((void *)(uintptr_t)map_base, PWMSS_PAGE_SIZE);
		close(fd);
		LOG_AND_EXIT("malloc failed for epwm_mmap_handle_t");
		return NULL;
	}

	handle->fd = fd;
	handle->map_base = map_base;
	handle->pwmss_regs = map_base + page_offset;
	handle->epwm_regs = handle->pwmss_regs + EPWM_MODULE_OFFSET;
	handle->channel = channel;
	handle->period_ns = 0U;

	LOG("ePWM mmap ready (VA %p, channel %s)", (const volatile void *)handle->epwm_regs, (channel == EPWM_CHANNEL_A) ? "A" : "B");
	return handle;
}

/*--------------------------------------
 * Function: epwm_mmap_close
 *--------------------------------------*/
void epwm_mmap_close(epwm_mmap_handle_t *handle) {
	if (handle == NULL) {
		return;
	}
	epwm_mmap_enable(handle, false);
	munmap((void *)(uintptr_t)handle->map_base, PWMSS_PAGE_SIZE);
	close(handle->fd);
	free(handle);
}

/*--------------------------------------
 * Function: epwm_mmap_set_period_ns
 *--------------------------------------*/
void epwm_mmap_set_period_ns(epwm_mmap_handle_t *handle, uint32_t period_ns) {
	uint32_t counts = period_ns / TBCLK_NS_PER_TICK;
	if (counts < 2U || counts > 65536U) {
		LOG_AND_EXIT("period_ns %u out of range for 320 ns/tick prescaler "
		             "(valid: 640 ns – 20 971 520 ns)",
		    period_ns);
		return;
	}
	handle->period_ns = period_ns;
	*reg16(handle->epwm_regs, EPWM_TBPRD_OFF) = (uint16_t)(counts - 1U);
}

/*--------------------------------------
 * Function: epwm_mmap_set_duty_ns
 *--------------------------------------*/
void epwm_mmap_set_duty_ns(epwm_mmap_handle_t *handle, uint32_t duty_ns) {
	if (handle->period_ns == 0U) {
		LOG_AND_EXIT("epwm_mmap_set_period_ns must be called before epwm_mmap_set_duty_ns");
		return;
	}
	uint32_t clamped = (duty_ns > handle->period_ns) ? handle->period_ns : duty_ns;
	uint16_t cmp = (uint16_t)(clamped / TBCLK_NS_PER_TICK);

	if (handle->channel == EPWM_CHANNEL_A) {
		*reg16(handle->epwm_regs, EPWM_CMPA_OFF) = cmp;
	} else {
		*reg16(handle->epwm_regs, EPWM_CMPB_OFF) = cmp;
	}
}

/*--------------------------------------
 * Function: epwm_mmap_enable
 *--------------------------------------*/
void epwm_mmap_enable(epwm_mmap_handle_t *handle, bool enable) {
	uint16_t aqcsfrc = *reg16(handle->epwm_regs, EPWM_AQCSFRC_OFF);

	if (enable) {
		/* Remove software force for this channel and start the counter */
		if (handle->channel == EPWM_CHANNEL_A) {
			aqcsfrc = (uint16_t)(aqcsfrc & ~AQCSFRC_CSFA_MASK);
		} else {
			aqcsfrc = (uint16_t)(aqcsfrc & ~AQCSFRC_CSFB_MASK);
		}
		*reg16(handle->epwm_regs, EPWM_AQCSFRC_OFF) = aqcsfrc;
		/* Build from TBCTL_INIT so prescaler bits are always our known-good values */
		*reg16(handle->epwm_regs, EPWM_TBCTL_OFF) = (uint16_t)((TBCTL_INIT & ~TBCTL_CTRMODE_MASK) | TBCTL_CTRMODE_UP);
	} else {
		/* Force output continuously low, then freeze the counter */
		if (handle->channel == EPWM_CHANNEL_A) {
			aqcsfrc = (uint16_t)((aqcsfrc & ~AQCSFRC_CSFA_MASK) | AQCSFRC_CSFA_LOW);
		} else {
			aqcsfrc = (uint16_t)((aqcsfrc & ~AQCSFRC_CSFB_MASK) | AQCSFRC_CSFB_LOW);
		}
		*reg16(handle->epwm_regs, EPWM_AQCSFRC_OFF) = aqcsfrc;
		*reg16(handle->epwm_regs, EPWM_TBCTL_OFF) = TBCTL_INIT;
	}
}
