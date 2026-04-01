#include "servo_controller.h"

#define SERVO_CHIP    (2U)
#define SERVO_CHANNEL ('b')

int main(void)
{
#ifdef NDEBUG
    servo_init(SERVO_CHIP, SERVO_CHANNEL);
#endif /* NDEBUG */

    return 0;
}
