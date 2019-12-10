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
#include <math.h>

#define MAX_CYCLES 6
#define MAX_PWM 63 // 2^MAX_CYCLES - 1

#define MAX_ACTIVITY 80
#define MIN_ACTIVITY 20
#define ACTIVITY_INCREMENT 5

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

enum functions
{
    STAY_IN_DETECTION,
    NO_FUNCTION,
    LED_TORCH_STATIC,
    FUNCTIONS_NUM
};

volatile bool leds_buffer[LED_NUM][MAX_CYCLES];
volatile bool leds_display[LED_NUM][MAX_CYCLES];
volatile bool updated;
volatile uint8_t led_vals[LED_NUM][4]; // 0 are current, 1 are desired, 2 is the rate of change, 3 effects

volatile int shake_count = 0;
volatile int rtc_alarm = 0;
volatile int activity = MIN_ACTIVITY;

uint8_t function = STAY_IN_DETECTION;


/* ===== WAKEUP/SLEEP FUNCTIONS ===== */

void power_down(void)
{
    HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, 0);
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, 0);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, 0);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, 0);
    HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, 0);
    HAL_GPIO_WritePin(LEDW_GPIO_Port, LEDW_Pin, 0);

    HAL_TIM_Base_Stop_IT(&htim1);
    HAL_TIM_Base_Stop_IT(&htim3);
}

void power_up(void)
{
    HAL_TIM_Base_Start_IT(&htim1);
    HAL_TIM_Base_Start_IT(&htim3);
}

void wait_for_leds(void)
{
    int check = 0;
    while (1)
    {
        check = 0;
        for (int i = 0; i < LED_NUM; i++)
            for (int j = 0; j < MAX_CYCLES; j++)
                check += leds_buffer[i][j];

        if (!check)
            break;
    }
    
}


/* ===== LED FUNCTIONS ===== */

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

void led_on_just_one(uint8_t led, int8_t value, int8_t rate)
{
    led_fade(led, value, rate);
    for (int i = LED_R; i < LED_NUM; i++)
    {
        if (i != led)
            led_fade(i, 0, rate);
    }
}

/* ===== INTERRUPTS AND USER INPUT FUNCTIONS ===== */

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
    }
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc)
{
    RTC_TimeTypeDef sTime = {0};
    sTime.Hours = 0x0;
    sTime.Minutes = 0x0;
    sTime.Seconds = 0x0;
    HAL_RTC_SetTime(hrtc, &sTime, RTC_FORMAT_BCD);

    HAL_RTC_DeactivateAlarm(hrtc, RTC_ALARM_A);
    
    RTC_AlarmTypeDef sAlarm = {0};
    sAlarm.AlarmTime.Hours = 0x0;
    sAlarm.AlarmTime.Minutes = 0x0;
    sAlarm.AlarmTime.Seconds = 0x15;
    sAlarm.AlarmDateWeekDay = 0x1;
    sAlarm.Alarm = RTC_ALARM_A;
    HAL_RTC_SetAlarm_IT(hrtc, &sAlarm, RTC_FORMAT_BCD);

    rtc_alarm++;
}

uint8_t button_state(void)
{
    return HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin);
}

/* ===== RNG FUNCTIONS ===== */

// returns 0 or 1 at random based on the midpoint
// midpoint 1-99 (%)
uint8_t percent_chance_bool(uint8_t midpoint)
{
    if(rand() % 100 > midpoint)
    {
        return 1;
    }
    return 0;
}

uint8_t random_in_range(uint8_t from, uint8_t to)
{
    return rand() % (to - from + 1) + from;
}

uint8_t random_in_range_exp(uint8_t from, uint8_t to)
{
    int range = to - from;
    int num = random_in_range(1, pow(4, range + 1));
    for (uint8_t i = 0; i < range + 1; i++)
    {
        if (num >= pow(4, i) && num < pow(4, i + 1))
            return range - i + from;
    }
    return from;
}

/* ===== MAIN ===== */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM1_Init();
    MX_TIM3_Init();
    MX_RTC_Init();

    power_up();

    srand(HAL_GetTick());
    
    // should only run once after wakeup
    // as it exits stop mode the corresponding interrupt routine will get carried out
    // and then it jumps into this loop
    while (1)
    {
        HAL_Delay(10); // so it can catch the shake iterrupst 
        power_up();

        if (shake_count)
        {
            shake_count = 0;
            led_fade_inout(LED_B, MAX_PWM, 4);
            if (activity < MAX_ACTIVITY)
            {
                activity += ACTIVITY_INCREMENT;
                led_fade_inout(LED_G, MAX_PWM, 4);
            }
        }
        if (rtc_alarm)
        {
            led_fade_inout(LED_O, MAX_PWM, 4);
            if (activity > MIN_ACTIVITY)
            {
                activity -= ACTIVITY_INCREMENT;
                led_fade_inout(LED_P, MAX_PWM, 4);
            }
        }

        // FIXME: it has to react to actvity + chance of not turning on at all
        /* int rand_count = random_in_range_exp(1, 3);
        for (int flash = 0; flash <= rand_count; flash++)
        {
            led_fade_inout(random_in_range(0, 5), random_in_range(30, MAX_PWM), random_in_range(1, 4));
        } */

        // detect the user input
        function = STAY_IN_DETECTION;
        int press_count = 0;
        // based on button state to save power
        if (button_state())
        {
            int timer = HAL_GetTick();
            int timer_last_pressed = HAL_GetTick();
            bool pressed = 0;
            bool released = 0;
            while (!function)
            {
                if (button_state())
                {
                    timer_last_pressed = HAL_GetTick();
                    pressed = 1;
                    if (HAL_GetTick() - timer > 100 && HAL_GetTick() - timer < 700)
                    {
                        led_on_just_one(press_count, 32, 2);
                    }
                    if (HAL_GetTick() - timer >= 700)
                    {
                        led_on_just_one(LED_W, 32, 2);
                    }
                    if (HAL_GetTick() - timer >= 1500)
                    {
                        function = LED_TORCH_STATIC;
                        break;
                    }
                }
                else
                {
                    released = 1; //TODO: implement it so if you just keep the button pressed it will only do the torch, go into the menu once you release the button for the first time
                    if (function == LED_TORCH_STATIC)
                    {
                        break;
                    }
                    else
                    {
                        if (pressed)
                        {
                            press_count++;
                            if (press_count > 5)
                            {
                                press_count = 0;
                            }
                            pressed = 0;
                            led_on_just_one(press_count - 1, 32, 2);
                        }
                        timer = HAL_GetTick();
                    }
                }

                if (HAL_GetTick() - timer_last_pressed > 1000)
                {
                    if (!press_count)
                    {
                        // breaks anyway so it will get out of the loop
                        function = NO_FUNCTION;
                    }
                    break;
                }
                HAL_Delay(10);
            }
        }

        if (function > NO_FUNCTION)
        {
            switch (function)
            {
                case LED_TORCH_STATIC:
                    shake_count = 0;
                    led_fade(LED_W, MAX_PWM, 3);
                    while (shake_count < 50) {
                        // due to no debouncing it can accumulate pretty quickly unitentionaly
                        if (shake_count > 5)
                            shake_count--;
                        HAL_Delay(100);
                    }
                    led_fade(LED_W, 0, 3);
                    break;
                
                default:
                    break;
            }
        }
        
        led_fade_inout(LED_R, MAX_PWM, 4);
        HAL_Delay(500);

        //wait_for_leds();
        power_down();

        rtc_alarm = 0;
        shake_count = 0;
        press_count = 0;


        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
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

/* for (int i = 0; i < LED_NUM - 1; i++)
    {
        led_fade_inout(i, 10, 1);
        HAL_Delay(50);
    } */
    //led_fade_inout(rand() % 6, MAX_PWM, rand() % 3 + 1);