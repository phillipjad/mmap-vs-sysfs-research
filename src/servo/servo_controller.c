#include "servo_controller.h"

#include <pthread.h>
#ifdef NDEBUG /* We only need PWM control in release */
#include "mmap_control.h"
#include "pwm_io_logic.h"
#endif /* NDEBUG */
#include "logger.h"
#include "project_constants.h"
#include "project_types.h"

/* Servo Angles */
#define SERVO_RIGHT (2000000U)
#define SERVO_CENTER (1500000U)
#define SERVO_LEFT (1000000U)

/* Servo Constants */
#define SERVO_PWM_NS (20000000U)
#define GATE_RAISE (SERVO_RIGHT)
#define GATE_LOWER (SERVO_CENTER)

#ifdef NDEBUG
static uint8_t extracted_chip_value = 0;
static uint8_t extracted_channel_value = 0;
static bool extracted_use_sysfs = true;

/**
 * @brief This function ensures that the pin is muxxed for pwm to prevent having
 * to manually run config-pin on the BBB
 *
 * @param[in] servo_chip The servo chip passed in by user input/config
 * @param[in] servo_channel The servo channel passed in by user input/config
 */
static void configure_pwm_pinmux(uint8_t servo_chip, char servo_channel) {
  const char *pin_name = NULL;
  if (servo_chip == 1) {
    if ((servo_channel == 'a') || (servo_channel == 'A')) {
      pin_name = "P9_14"; /* EHRPWM1A */
    } else if ((servo_channel == 'b') || (servo_channel == 'B')) {
      pin_name = "P9_16"; /* EHRPWM1B */
    } else {
      /* MISRA requires else */
    }
  } else if (servo_chip == 2) {
    if ((servo_channel == 'a') || (servo_channel == 'A')) {
      pin_name = "P8_19"; /* EHRPWM2A */
    } else if ((servo_channel == 'b') || (servo_channel == 'B')) {
      pin_name = "P8_13"; /* EHRPWM2B */
    } else {
      /* MISRA requires else */
    }
  } else {
    /* MISRA requires else */
  }

  if (pin_name == NULL) {
    LOG_AND_EXIT("ERROR: Could not resolve BBB pin for chip %d channel %c",
                 servo_chip, servo_channel);
    return;
  }

  char path[MAX_FILENAME_LENGTH + 1U] = {0};
  (void)snprintf(path, MAX_FILENAME_LENGTH,
                 "/sys/devices/platform/ocp/ocp:%s_pinmux/state", pin_name);

  FILE *pinmux_file = fopen(path, "w");
  if (pinmux_file == NULL) {
    LOG_AND_EXIT("ERROR: Failed to open pinmux state file: %s", path);
    return;
  }
  if (fprintf(pinmux_file, "pwm") < 0) {
    (void)fclose(pinmux_file);
    LOG_AND_EXIT("ERROR: Failed to write pwm to pinmux state file: %s", path);
    return;
  }
  (void)fclose(pinmux_file);
}

static void servo_init_hw(uint8_t servo_chip, char servo_channel,
                          bool use_sysfs) {
  // Store mapped values
  // Store chip value
  if (servo_chip == 1) {
    extracted_chip_value = (uint8_t)4; /* EHRPWM1 maps to pwmchip4 */
  } else if (servo_chip == 2) {
    extracted_chip_value = (uint8_t)7; /* EHRPWM2 maps to pwmchip7 */
  } else {
    LOG_AND_EXIT("ERROR: Invalid EHRPWM chip value given in hardware init.");
    return;
  }
  // Get mapped channel uint8_t value
  if ((servo_channel == 'a') || (servo_channel == 'A')) {
    extracted_channel_value = (uint8_t)0;
  } else if ((servo_channel == 'b') || (servo_channel == 'B')) {
    extracted_channel_value = (uint8_t)1;
  } else {
    LOG_AND_EXIT("ERROR: Invalid EHRPWM channel value given in hardware init.");
    return;
  }
  // Store if we are using sysfs or mmap
  extracted_use_sysfs = use_sysfs;

  // Configure pin mux before accessing PWM sysfs
  configure_pwm_pinmux(servo_chip, servo_channel);
  // sysfs init
  if (use_sysfs) {
    // Export the PWM channel
    init_pwm_channel(extracted_chip_value, extracted_channel_value);
    // Set period
    set_pwm_period(extracted_chip_value, extracted_channel_value, SERVO_PWM_NS);
    // Set duty cycle
    set_pwm_duty_cycle(extracted_chip_value, extracted_channel_value,
                       GATE_RAISE);
    // Enable output
    enable_pwm(extracted_chip_value, extracted_channel_value, true);
    LOG("SYSFS init complete");
  } else { // mmap init
    // Export the PWM channel
    pwm_mmap_init(extracted_chip_value, extracted_channel_value);
    // Set period
    mmap_set_pwm_period(extracted_chip_value, extracted_channel_value,
                        SERVO_PWM_NS);
    // Set duty cycle
    mmap_set_duty_cycle(extracted_chip_value, extracted_channel_value,
                        GATE_RAISE);
    // Enable output
    mmap_enable_pwm(extracted_chip_value, extracted_channel_value, true);
    LOG("MMAP init complete");
  }
}
#else
static void servo_init_sw() { LOG("Servo initialized"); }
#endif /* NDEBUG */

/*--------------------------------------
 * Function: servo_init
 *--------------------------------------*/
void servo_init(uint8_t servo_chip, char servo_channel, bool use_sysfs) {
#ifdef NDEBUG
  servo_init_hw(servo_chip, servo_channel, use_sysfs);
#else
  (void)servo_chip;
  (void)servo_channel;
  servo_init_sw();
#endif /* NDEBUG */
}

/*--------------------------------------
 * Function: servo_raise
 *--------------------------------------*/
void servo_raise(void) {
#ifdef NDEBUG
  // Set duty cycle
  if (extracted_use_sysfs) {
    set_pwm_duty_cycle(extracted_chip_value, extracted_channel_value,
                       GATE_RAISE);
  } else {
    mmap_set_duty_cycle(extracted_chip_value, extracted_channel_value,
                        GATE_RAISE);
  }
#else
  LOG("Raising gate");
#endif /* NDEBUG */
}

/*--------------------------------------
 * Function: servo_lower
 *--------------------------------------*/
void servo_lower(void) {
#ifdef NDEBUG
  // Set duty cycle
  if (extracted_use_sysfs) {
    set_pwm_duty_cycle(extracted_chip_value, extracted_channel_value,
                       GATE_LOWER);
  } else {
    mmap_set_duty_cycle(extracted_chip_value, extracted_channel_value,
                        GATE_LOWER);
  }
#else
  LOG("Lowering gate");
#endif /* NDEBUG */
}

/*--------------------------------------
 * Function: servo_shutdown
 *--------------------------------------*/
void servo_shutdown(void) {
  LOG("Shutting down gate");
#ifdef NDEBUG
  // Set duty cycle
  if (extracted_use_sysfs) {
    set_pwm_duty_cycle(extracted_chip_value, extracted_channel_value,
                       GATE_RAISE);
  } else {
    mmap_set_duty_cycle(extracted_chip_value, extracted_channel_value,
                        GATE_RAISE);
  }
  struct timespec timer = {0};
  timer.tv_sec = 0;
  timer.tv_nsec = SEC_TO_NSEC / 2;
  nanosleep(&timer, NULL);
  // Disable output and unexport
  if (extracted_use_sysfs) {
    enable_pwm(extracted_chip_value, extracted_channel_value, false);
    unexport_pwm_channel(extracted_chip_value, extracted_channel_value);
  } else {
    mmap_enable_pwm(extracted_chip_value, extracted_channel_value, false);
    mmap_unexport_pwm_channel(extracted_chip_value, extracted_channel_value);
  }
#endif /* NDEBUG */
}
