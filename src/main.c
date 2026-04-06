#include <unistd.h>
#include "servo_controller.h"

#define SERVO_CHIP (2U)
#define SERVO_CHANNEL ('b')

int main(void) {
#ifdef NDEBUG
  servo_init(SERVO_CHIP, SERVO_CHANNEL, false);
#endif /* NDEBUG */
  sleep(1);
  servo_raise();
  sleep(1);
  servo_lower();
  sleep(1);
  servo_shutdown();
  return 0;
}
