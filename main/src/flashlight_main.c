/*
 *  flashlight_main.c
 *
 *  Created on: 5 Dec 2019
 *  Author: Jiri Manak
 */

#include "main.h"
#include "flashlight_main.h"
#include <stdbool.h>
#include <string.h>

#define MAX_CYCLES 6
#define MAX_PWM 63 // 2^MAX_CYCLES - 1

enum led_id
{
    LED_R,
    LED_O,
    LED_Y,
    LED_G,
    LED_B,
    LED_P,
    LED_W,
    LED_NUM
};

volatile bool leds_buffer[LED_NUM][MAX_CYCLES];
volatile bool leds_display[LED_NUM][MAX_CYCLES];
volatile bool updated = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        static const int prescaler_sequence[] = {10, 20, 40, 80, 160, 320};
        static int cycle = 0;

        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, leds_buffer[LED_R][cycle]);
        HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, leds_buffer[LED_O][cycle]);
        HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, leds_buffer[LED_Y][cycle]);
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, leds_buffer[LED_G][cycle]);
        HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, leds_buffer[LED_B][cycle]);
        HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, leds_buffer[LED_P][cycle]);
        HAL_GPIO_WritePin(LEDW_GPIO_Port, LEDW_Pin, leds_buffer[LED_W][cycle]);

        cycle++;
        if (cycle >= MAX_CYCLES)
            cycle = 0;

        TIM1->PSC = (uint32_t)(prescaler_sequence[cycle]);

        /* if (updated)
        {
            updated = 0;
            memcpy(leds_display, leds_buffer, sizeof(leds_display));
        } */
    }
    if (htim->Instance == TIM3)
    {
        
    }
}

void led_write_pwm(uint8_t led, int8_t value)
{
    for (int i = 0; i < MAX_CYCLES; i++)
    {
        leds_buffer[led][i] = value & 1;
        value >>= 1;
    }
    updated = 1;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM1_Init();
    //MX_TIM3_Init();

    HAL_TIM_Base_Start_IT(&htim1);

    while(1)
    {
        HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
        /*HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
        HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
        HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
        HAL_GPIO_TogglePin(LED4_GPIO_Port, LED4_Pin);
        HAL_GPIO_TogglePin(LED5_GPIO_Port, LED5_Pin);
        HAL_GPIO_TogglePin(LEDW_GPIO_Port, LEDW_Pin); */

        for (int i = 0; i < 63; i++)
        {
            led_write_pwm(LED_G, i);
            led_write_pwm(LED_R, i);
            HAL_Delay(10);
        }
        for (int i = 63; i >= 0; i--)
        {
            led_write_pwm(LED_G, i);
            led_write_pwm(LED_R, i);
            HAL_Delay(10);
        }

        HAL_Delay(1000);
    }
}