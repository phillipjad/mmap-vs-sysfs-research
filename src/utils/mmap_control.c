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
/* Base physical address for the PWM Subsystem 2 base address (from TRM memory
 * map) */
#define EPWM2B_BASE_ADDR (0x48304000)
/* Offsets for registers within PWM module (from TRM register map) */
#define PWMSS2_CONFIG_REGISTER_OFF (0x0)
#define PWMSS2_ECAP2_REGISTER_OFF (0x100)
#define PWMSS2_EQEP2_REGISTER_OFF (0x180)
#define PWMSS2_EPWM2_REGISTER_OFF (0x200)

/* CM_PER Constants */
#define CM_PER_BASE (0x44E00000)      // pg 179 in manual
#define CM_PER_EPWMSS2_CLKCTRL (0xD8) // pg 1292 in manual
#define CM_PER_ENABLE (0x2)           // pg 1292 in manual

/* PWMSS_CTRL Constants */
#define CONTROL_MODULE_BASE (0x44E10000) // pg 180 in manual
#define PWMSS_CTRL_OFFSET (0x664)        // pg 1496 in manual
#define PWMSS_ENABLE                                                           \
  (1 << 2) // pg 1496 in maual - Timebase clock enable for PWMSS2

/* PWMSS2 Constants */
#define PWMSS2_BASE (0x48304000)       // pg 184 in manual
#define PWMSS2_CLKCONFIG_OFFSET (0x08) // pg 2332 in manual
#define PWMSS2_ENABLE (1 << 8)         // pg 2332 in manual ePWMCLK_EN field

/* EPWM Register Constants */
#define TBPRD_OFFSET (0x0A)  // pg 2433 in manual
#define TBCTL_OFFSET (0x00)  // pg 2433 in manual
#define CMPB_OFFSET (0x14)   // pg 2433 in manual
#define AQCTLB_OFFSET (0x18) // pg 2433 in manual

// PWMSS Clock Signals at 100MHz & servo duty cycle is 20ms
// 1 tick => 10 nanoseconds
// Bcs register of tbprd is 16 bits, epwm2_tbctl needs to be / 128
// 100MHz / 128 = 781.25 kHz
// 1 sec / 781,250 = 1280 nanosec
#define NS_PER_TICK (1280U)

// Helper to access 32-bit registers by offset
static inline volatile uint32_t *reg32(volatile uint8_t *base, uint32_t off) {
  return (volatile uint32_t *)((uintptr_t)base + off);
}

// Helper to access 16-bit registers by offset
static inline volatile uint16_t *reg16(volatile uint8_t *base, uint16_t off) {
  return (volatile uint16_t *)((uintptr_t)base + off);
}

// mmap memory variables
static volatile uint8_t *cm_per_base = NULL;
static volatile uint8_t *pwmss2_base = NULL;
static volatile uint8_t *control_module_map_base = NULL;

// Defined in EPWM Regsiter - pg 2433
static volatile uint16_t *epwm2_tbctl = NULL;      // EPWM Register TBCTL
static volatile uint16_t *epwm2_period = NULL;     // EPWM Register TBPRD
static volatile uint16_t *epwm2_duty_cycle = NULL; // EPWM Register CMPB
static volatile uint16_t *epwm2_aqctlb = NULL;     // EPWM Register AQCTLB

/*--------------------------------------
 * Function: map_memory
 *--------------------------------------*/
static volatile uint8_t *map_memory(uint32_t address) {
  // Page base
  uint32_t page_base = 0;
  // Page offset
  uint32_t page_off = 0;

  // mmap Setup
  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) {
    LOG_AND_EXIT("Failed to open /dev/mem for mmap");
  }

  page_base = (uint32_t)(address & ~(PAGE_SIZE - 1U));
  page_off = (uint32_t)(address - page_base);

  volatile uint8_t *map_base = (volatile uint8_t *)mmap(
      NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_base);
  if (map_base == MAP_FAILED) {
    (void)close(fd);
    LOG_AND_EXIT("Failed to create mmap for physical address: 0x%x", address);
    return NULL;
  }
  (void)close(fd);
  return map_base + page_off;
}

void pwm_mmap_init(uint8_t chip, uint8_t channel) {
  if ((chip != 7) || (channel != 1)) {
    LOG_AND_EXIT(
        "mmap_control only initializes pin EHRPWM2B (Chip 7, Channel 1)");
    return;
  }

  // Manage the PWMSS2:
  // https://www.ti.com/lit/ug/spruh73q/spruh73q.pdf?ts=1771861819289 - page
  // 1292
  cm_per_base = map_memory(CM_PER_BASE);
  volatile uint32_t *cm_pwmss2_clkctrl =
      reg32(cm_per_base, CM_PER_EPWMSS2_CLKCTRL);
  *cm_pwmss2_clkctrl = CM_PER_ENABLE;

  // Timebase clock enable for PWMSS2
  control_module_map_base = map_memory(CONTROL_MODULE_BASE);
  volatile uint32_t *pwmss_ctrl_module =
      reg32(control_module_map_base, PWMSS_CTRL_OFFSET);
  *pwmss_ctrl_module |= PWMSS_ENABLE;

  // Configure Clock
  pwmss2_base = map_memory(PWMSS2_BASE);
  volatile uint32_t *cm_pwmss2_clkcnfg =
      reg32(pwmss2_base, PWMSS2_CLKCONFIG_OFFSET);
  *cm_pwmss2_clkcnfg |= PWMSS2_ENABLE;

  // Pin setup
  volatile uint8_t *epwm2_base = pwmss2_base + PWMSS2_EPWM2_REGISTER_OFF;
  epwm2_period = reg16(epwm2_base, TBPRD_OFFSET);
  epwm2_tbctl = reg16(epwm2_base, TBCTL_OFFSET);
  epwm2_duty_cycle = reg16(epwm2_base, CMPB_OFFSET);
  epwm2_aqctlb = reg16(epwm2_base, AQCTLB_OFFSET);

  // Setup clock prescale field for CLKDIV - pg 2434
  *epwm2_tbctl = (7 << 10);
  // Stop counter
  *epwm2_tbctl |= (0x3);
}

/*--------------------------------------
 * Function: mmap_map_close
 *--------------------------------------*/
void mmap_map_close(void) {
  if (cm_per_base != NULL) {
    munmap((void *)((uintptr_t)cm_per_base & ~(PAGE_SIZE - 1U)), PAGE_SIZE);
  }
  if (pwmss2_base != NULL) {
    munmap((void *)((uintptr_t)pwmss2_base & ~(PAGE_SIZE - 1U)), PAGE_SIZE);
  }
  if (control_module_map_base != NULL) {
    munmap((void *)((uintptr_t)control_module_map_base & ~(PAGE_SIZE - 1U)),
           PAGE_SIZE);
  }
}

void mmap_set_pwm_period(uint32_t period_ns) {
  if (epwm2_period != NULL) {
    *epwm2_period = (uint16_t)(period_ns / NS_PER_TICK); // 2.4MHz
  }
}

void mmap_set_duty_cycle(uint32_t duty_ns) {
  if (epwm2_duty_cycle != NULL) {
    *epwm2_duty_cycle = (uint16_t)(duty_ns / NS_PER_TICK); // 2.4MHz
  }
}

void mmap_enable_pwm(bool enable) {
  if (epwm2_aqctlb != NULL && epwm2_tbctl != NULL) {
    if (enable) {
      *epwm2_aqctlb = 0x0201; // set duty cycle 0x02 and clear ZRO field 0x01
      *epwm2_tbctl &= ~(0x3); // start counter
    } else {
      *epwm2_aqctlb = 0x0101; // clear duty cycle 0x01 and clear ZRO field 0x01
      *epwm2_tbctl |= (0x3);  // stop counter
    }
  }
}

void mmap_unexport_pwm_channel(void) {
  mmap_enable_pwm(false);
  epwm2_period = NULL;
  epwm2_duty_cycle = NULL;
  epwm2_aqctlb = NULL;
  epwm2_tbctl = NULL;
  mmap_map_close();
}
