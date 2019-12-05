/*
 *  flashlight_main.c
 *
 *  Created on: 28 Jul 2019
 *  Author: Jiri Manak
 */

#include "main.h"
#include "flashlight_main.h"

int main(void)
{
    SystemClock_Config();
    MX_GPIO_Init();


    while(1)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(100);
    }
}