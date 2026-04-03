#include "mmap_control.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "logger.h"
#include "project_constants.h"
#include "project_types.h"

/* PWMSS2 mmap constants */
#define PAGE_SIZE (4096U)
/* Base physical address for the PWM Subsystem 2 base address (from TRM memory map) */
#define EPWM2B_BASE_ADDR (0x48304000)
/* Offsets for registers within PWM module (from TRM register map) */
#define PWMSS2_CONFIG_REGISTER_OFF (0x0)
#define PWMSS2_ECAP2_REGISTER_OFF (0x100)
#define PWMSS2_EQEP2_REGISTER_OFF (0x180)
#define PWMSS2_EPWM2_REGISTER_OFF (0x200)

// Helper to access 32-bit registers by offset
static inline volatile uint32_t *reg32(volatile uint8_t *base, uint32_t off) {
	return (volatile uint32_t *)((uintptr_t)base + off);
}

/*---------------------------------------------
 * Function: get_mmap_base - helper function
 *---------------------------------------------*/
static volatile uint8_t *get_mmap_base(uint8_t pin) {
	uint8_t mmap_base_nubmer = pin / REGISTERS_PER_GROUP;
	if (mmap_base_nubmer >= mmap_COUNT || mmaps_array[mmap_base_nubmer].mmap_base == NULL ||
	    mmaps_array[mmap_base_nubmer].fd <= 0) {
		LOG_AND_EXIT("Failed to get mmap base for pin %d", pin);
		return NULL;
	} else {
		return mmaps_array[mmap_base_nubmer].mmap_base;
	}
}

/*--------------------------------------
 * Function: mmap_map_init
 *--------------------------------------*/
void mmap_map_init(void) {
	// Page base
	uint32_t page_base = 0;
	// Page offset
	uint32_t page_off = 0;
	// Clear out
	memset(mmaps_array, 0, sizeof(mmaps_array));

	// mmaps
	const uint32_t addresses[mmap_COUNT] = { mmap0_BASE_PHYS, mmap1_BASE_PHYS, mmap2_BASE_PHYS, mmap3_BASE_PHYS };

	// mmap Setup
	for (int i = 0; i < mmap_COUNT; ++i) {
		mmaps_array[i].fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (mmaps_array[i].fd < 0) {
			LOG_AND_EXIT("Failed to open /dev/mem for mmap PHYSICAL BASE: %d", i);
		}

		page_base = (uint32_t)(addresses[i] & ~(PAGE_SIZE - 1U));
		page_off = (uint32_t)(addresses[i] - page_base);

		mmaps_array[i].map_base =
		    (volatile uint8_t *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, mmaps_array[i].fd, page_base);
		if (mmaps_array[i].map_base == MAP_FAILED) {
			LOG_AND_EXIT("Failed to create mmap base for mmap PHYSICAL BASE: %d, at physical address: 0x%X", i, addresses[i]);
		}

		mmaps_array[i].mmap_base = mmaps_array[i].map_base + page_off;
	}
}

/*--------------------------------------
 * Function: mmap_map_close
 *--------------------------------------*/
void mmap_map_close(void) {
	for (int i = 0; i < mmap_COUNT; ++i) {
		if (mmaps_array[i].map_base && mmaps_array[i].map_base != MAP_FAILED) {
			munmap((void *)(uintptr_t)mmaps_array[i].map_base, PAGE_SIZE);
		}
		if (mmaps_array[i].fd >= 0) {
			close(mmaps_array[i].fd);
		}
	}
}

/*--------------------------------------
 * Function: mmap_set
 *--------------------------------------*/
void mmap_set(uint8_t pin, bool value) {
	// Get Base Address - exits if function fails
	volatile uint8_t *base = get_mmap_base(pin);

	// Calculate register number under mmap Group
	uint8_t pin_number = pin % REGISTERS_PER_GROUP;

	// Depending on bool value can put register to 1 using set or 0 using clear
	// Get registers values
	volatile uint32_t *register_address = reg32(base, value ? mmap_SETDATAOUT_OFF : mmap_CLEARDATAOUT_OFF);
	// Set register value to 1
	*register_address = (1U << pin_number);
}

/*--------------------------------------
 * Function: mmap_read
 *--------------------------------------*/
uint8_t mmap_read(uint8_t pin) {
	// Get Base Address - exits if function fails
	volatile uint8_t *base = get_mmap_base(pin);

	// Calculate register number under mmap Group
	uint8_t pin_number = pin % REGISTERS_PER_GROUP;

	// Get registers values
	volatile uint32_t *register_address = reg32(base, mmap_DATAIN_OFFSET);
	// Read if register valjue is populated after bitmask, if so then return true
	return (*register_address & (1U << pin_number)) ? 1U : 0U;
}

/*--------------------------------------
 * Function: mmap_clear
 *--------------------------------------*/
void mmap_clear(uint8_t pin) {
	// Get Base Address - exits if function fails
	volatile uint8_t *base = get_mmap_base(pin);

	// Calculate register number under mmap Group
	uint8_t pin_number = pin % REGISTERS_PER_GROUP;

	// Get registers values and use clear offset
	volatile uint32_t *register_address = reg32(base, mmap_CLEARDATAOUT_OFF);
	// Set register value to 1 to clear
	*register_address = (1U << pin_number);
}

/*--------------------------------------
 * Function: mmap_set_direction
 *--------------------------------------*/
void mmap_set_direction(uint8_t pin, const char *direction) {
	// Set mmap direction based on direction char
	if (strcmp(direction, mmap_IN) == 0) {
		// Set mmap to be input
		mmap_set_direction_in(pin);
	} else {
		// Set mmap to be output
		mmap_set_direction_out(pin);
	}
}

/*--------------------------------------
 * Function: mmap_set_direction_out
 *--------------------------------------*/
void mmap_set_direction_out(uint8_t pin) {
	// Get Base Address - exits if function fails
	volatile uint8_t *base = get_mmap_base(pin);

	// Calculate register number under mmap Group
	uint8_t pin_number = pin % REGISTERS_PER_GROUP;

	// Get register values
	volatile uint32_t *register_address = reg32(base, MMAP_OE_OFFSET);

	// Set register to value 0 to set as output pin
	*register_address &= ~(1U << pin_number);
}

/*--------------------------------------
 * Function: mmap_set_direction_in
 *--------------------------------------*/
void mmap_set_direction_in(uint8_t pin) {
	// Get Base Address - exits if function fails
	volatile uint8_t *base = get_mmap_base(pin);

	// Calculate register number under mmap Group
	uint8_t pin_number = pin % REGISTERS_PER_GROUP;

	// Get register values
	volatile uint32_t *register_address = reg32(base, mmap_OE_OFFSET);

	// Set register to value 1 to set as output pin
	*register_address |= (1U << pin_number);
}
