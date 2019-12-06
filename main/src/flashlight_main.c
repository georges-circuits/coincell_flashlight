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
volatile bool updated;
volatile uint8_t led_vals[LED_NUM][4]; // 0 are current, 1 are desired, 2 is the rate of change, 3 effects

void led_write_pwm(uint8_t led, int8_t value)
{
    for (int i = 0; i < MAX_CYCLES; i++)
    {
        leds_buffer[led][i] = value & 1;
        value >>= 1;
    }
    updated = 1;
}

void led_fade(uint8_t led, int8_t value, int8_t rate)
{
    led_vals[led][1] = value;
    led_vals[led][2] = rate;
}

void led_fade_inout(uint8_t led, int8_t value, int8_t rate)
{
    led_vals[led][1] = value;
    led_vals[led][2] = rate;
    led_vals[led][3] = 1;
}

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
        //HAL_GPIO_TogglePin(LED5_GPIO_Port, LED5_Pin);
        for (int i = 0; i < LED_NUM; i++)
        {
            if (led_vals[i][2])
            {
                // in case the value is so close to desired it would overshoot
                if ((led_vals[i][0] > led_vals[i][1] - led_vals[i][2] && 
                    led_vals[i][0] < led_vals[i][1] + led_vals[i][2]) ||
                    led_vals[i][0] >= MAX_PWM)
                {
                    led_vals[i][0] = led_vals[i][1];
                }
                
                if (led_vals[i][0] > led_vals[i][1])
                    led_vals[i][0] -= led_vals[i][2];
                
                if (led_vals[i][0] < led_vals[i][1])
                    led_vals[i][0] += led_vals[i][2];
                
                // resets the rate of change value to 0 once the fade is complete
                if (led_vals[i][0] == led_vals[i][1])
                {
                    if (led_vals[i][3])
                    {
                        // after fade in fade back out
                        led_vals[i][0] -= led_vals[i][2];
                        led_vals[i][1] = 0;
                        led_vals[i][3] = 0;
                    }
                    else
                    {
                        led_vals[i][2] = 0;
                    }
                }
                
                led_write_pwm(i, led_vals[i][0]);            
            }
        }
        
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM1_Init();
    MX_TIM3_Init();

    HAL_TIM_Base_Start_IT(&htim1);
    HAL_TIM_Base_Start_IT(&htim3);

    while(1)
    {
        for (int i = 0; i < LED_NUM - 1; i++)
        {
            led_fade_inout(i, MAX_PWM, 2);
            HAL_Delay(200);
        }
        

        HAL_Delay(2000);
    }
}