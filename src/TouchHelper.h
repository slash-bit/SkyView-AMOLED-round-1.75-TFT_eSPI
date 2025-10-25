#ifndef TOUCH_HELPER_H
#define TOUCH_HELPER_H

#define TOUCH_HIT_RADIUS 199 // Radius for touch hit detection in meters
void Touch_setup();
void touchTask(void *pvParameters);
extern void touchWakeUp();


#endif // TOUCH_HELPER_H