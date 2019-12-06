/*
 *  flashlight_main.c
 *
 *  Created on: 5 Dec 2019
 *  Author: Jiri Manak
 */

#include "main.h"
#include "flashlight_main.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM1_Init();
    //MX_TIM3_Init();


    while(1)
    {
        HAL_GPIO_TogglePin(LED0_Pin, LED0_GPIO_Port);
        HAL_Delay(100);
    }
}