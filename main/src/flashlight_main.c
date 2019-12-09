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
#include <stdio.h>
#include <stdlib.h>
//#include <math.h>

#define MAX_CYCLES 6
#define MAX_PWM 63 // 2^MAX_CYCLES - 1

#define FLASH_START 0x08003000 //sector 3

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

volatile int shake_count = 0;

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

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == SHAKE_IRQ_Pin)
    {
        shake_count++;
        led_fade_inout(LED_B, MAX_PWM, 2);
    }
}

/* void watchdog_set(int window, int reload)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Window = window;
    hiwdg.Init.Reload = reload;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
    {
        Error_Handler();
    }
    HAL_IWDG_Refresh(&hiwdg);
}

int watchdog_get_window(void)
{
    return hiwdg.Instance->WINR;
}

int watchdog_get_reload(void)
{
    return (int)hiwdg.Instance->RLR;
} */

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
    led_fade_inout(LED_O, MAX_PWM, 1);

    RTC_TimeTypeDef sTime = {0};
    sTime.Hours = 0x0;
    sTime.Minutes = 0x0;
    sTime.Seconds = 0x0;
    HAL_RTC_SetTime(hrtc, &sTime, RTC_FORMAT_BCD);

    RTC_AlarmTypeDef sAlarm = {0};
    sAlarm.AlarmTime.Hours = 0x0;
    sAlarm.AlarmTime.Minutes = 0x0;
    sAlarm.AlarmTime.Seconds = 0x15;
    sAlarm.AlarmDateWeekDay = 0x1;
    sAlarm.Alarm = RTC_ALARM_A;
    HAL_RTC_SetAlarm_IT(hrtc, &sAlarm, RTC_FORMAT_BCD);
}



int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    //MX_IWDG_Init();
    MX_TIM1_Init();
    MX_TIM3_Init();
    MX_RTC_Init();

    HAL_TIM_Base_Start_IT(&htim1);
    HAL_TIM_Base_Start_IT(&htim3);

    /* int reload = watchdog_get_reload();
    int window = watchdog_get_window();
    watchdog_set(4094, 4000); */

    HAL_Delay(10);

    srand(HAL_GetTick());
    /* for (int i = 0; i < LED_NUM - 1; i++)
    {
        led_fade_inout(i, 10, 1);
        HAL_Delay(50);
    } */
    //led_fade_inout(rand() % 6, MAX_PWM, rand() % 3 + 1);

    while(1)
    {

        if (HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin))
        {
            led_fade_inout(rand() % 6, MAX_PWM, rand() % 3 + 1);
        }
        
        //led_fade_inout(test_r, MAX_PWM, 2);

        //HAL_IWDG_Refresh(&hiwdg);
        

        HAL_Delay(200);
    }

    led_fade_inout(LED_R, MAX_PWM, 2);
    HAL_Delay(500);

    /* HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU); 
    // Enable WKUP pin
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    HAL_SuspendTick();
    HAL_PWR_EnterSTANDBYMode(); */

    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

}