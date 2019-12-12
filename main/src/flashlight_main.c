/*
 *  flashlight_main.c
 *
 *  Created on: 5 Dec 2019
 *  Author: Jiri Manak
 */

#include "main.h"
#include "flashlight_main.h"

#include <stdbool.h>
//#include <string.h>
#include <stdio.h>
#include <stdlib.h>
//#include <math.h>

#define MAX_CYCLES 6
#define MAX_PWM 63 // 2^MAX_CYCLES - 1

#define MAX_ACTIVITY 75
#define MIN_ACTIVITY 25

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
    EXIT_MENU,
    LED_TORCH_SIGNAL,
    DISPLAY_TEMP,
    DISPLAY_BATTERY,
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
int supress_flash = 0;

/* ===== OTHER FUNCTIONS ===== */

int my_pow(int base, int x)
{
    int a = 1;
    for (int i = 0; i < x; i++)
    {
        a *= base;
    }
    return a;
}

int my_abs(int a)
{
    if (a < 0)
        return a * -1;
    return a;
}

/* ===== WAKEUP/SLEEP FUNCTIONS ===== */

void power_down(void)
{
    HAL_TIM_Base_Stop_IT(&htim1);
    HAL_TIM_Base_Stop_IT(&htim3);

    HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, 0);
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, 0);
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, 0);
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, 0);
    HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, 0);
    HAL_GPIO_WritePin(LEDW_GPIO_Port, LEDW_Pin, 0);
}

void power_up(void)
{
    HAL_TIM_Base_Start_IT(&htim1);
    HAL_TIM_Base_Start_IT(&htim3);
}

void wait_for_leds(void)
{
    int check = 0;
    int timer = HAL_GetTick();
    while (1)
    {
        check = 0;
        /* for (int i = 0; i < LED_NUM; i++)
            for (int j = 0; j < MAX_CYCLES; j++)
                check += leds_buffer[i][j]; */
        
        for (int i = 0; i < LED_NUM; i++)
        {
            check += led_vals[i][0];
        }

        if (check == 0)
            break;

        if(HAL_GetTick() - timer > 2000)
        {
           display_error(5);
           break;
        }
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
    if (led_vals[led][0] != value)
    {
        led_fade(led, value, rate);
    }
    for (int i = LED_R; i < LED_NUM; i++)
    {
        if (i != led)
            led_fade(i, 0, rate);
    }
}

void leds_all_off(void)
{
    for (int i = LED_R; i < LED_NUM; i++)
    {
        led_fade(i, 0, 5);
    }
}

void display_error(uint8_t pulses)
{
    for (int i = 0; i < pulses + 1; i++)
    {
        led_fade_inout(LED_R, MAX_PWM, 15);
        HAL_Delay(150);
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
                    led_vals[i][0] > MAX_PWM)
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
    if (GPIO_Pin == BUTTON_IRQ_Pin)
    {
        // used to be shake_count++;
        // now it's just a wakeup IRQ
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

void update_shake(int time)
{
    static bool old;
    static bool new;
    int timer = HAL_GetTick();
    while (HAL_GetTick() - timer < time)
    {
        new = HAL_GPIO_ReadPin(SHAKE_GPIO_Port, SHAKE_Pin);
        if (new != old)
            shake_count++;
        old = new;
        HAL_Delay(1);
    }
}

/* ===== RNG FUNCTIONS ===== */

// returns 0 or 1 at random based on the midpoint
// midpoint 1-99 (%)
uint8_t percent_chance_bool(uint8_t midpoint)
{
    if(rand() % 100 < midpoint)
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
    int num = random_in_range(1, my_pow(4, range + 1));
    for (uint8_t i = 0; i < range + 1; i++)
    {
        if (num >= my_pow(4, i) && num < my_pow(4, i + 1))
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
    MX_ADC_Init();
    MX_DMA_Init();

    power_up();

    srand(HAL_GetTick());
    
    // should only run once after wakeup
    // as it exits stop mode the corresponding interrupt routine will get carried out
    // and then it jumps into this loop
    while (1)
    {
        //HAL_Delay(5); // so it can catch the shake iterrupts
        //if (!button_state() || rtc_alarm)
        update_shake(100);

        power_up();

        if (shake_count > 1 || button_state())
        {
            //shake_count = 0;
            //led_fade_inout(LED_B, MAX_PWM, 4);
            if (activity < MAX_ACTIVITY)
            {
                activity += random_in_range(2, 15); //(my_abs(MAX_ACTIVITY - activity) / 10)
                //led_fade_inout(LED_G, MAX_PWM, 4);
            }
        }
        else //if (rtc_alarm)
        {
            //led_fade_inout(LED_O, MAX_PWM, 4);
            if (activity > MIN_ACTIVITY)
            {
                activity -= random_in_range(1, 3);
                //led_fade_inout(LED_P, MAX_PWM, 4);
            }
        }

        // detect the user input
        function = STAY_IN_DETECTION;
        int press_count = 0;
        // based on button state to save power
        if (button_state() && shake_count)
        {
            int timer = HAL_GetTick();
            int timer_last_pressed = HAL_GetTick();
            bool pressed = 0;
            bool released = 0;
            bool not_released = 1;
            bool pass[2] = {1, 1};
            supress_flash = 5;
            while (1)
            {
                if (button_state())
                {
                    timer_last_pressed = HAL_GetTick();
                    pressed = 1;
                    if (not_released)
                    {
                        if (HAL_GetTick() - timer >= 100 && pass[0])
                        {
                            pass[0] = 0;
                            led_on_just_one(LED_W, 12, 1);
                        }
                    }
                    else
                    {
                        if (HAL_GetTick() - timer >= 700 && pass[1])
                        {
                            pass[1] = 0;
                            led_on_just_one(LED_W, 12, 1);
                            press_count = 0;
                        }
                        if (released)
                        {
                            released = 0;
                            press_count++;
                            if (press_count > 5)
                            {
                                press_count = 0;
                            }
                        }
                    }
                    if (HAL_GetTick() - timer >= 1500)
                    {
                        function = LED_TORCH_STATIC;
                        break;
                    }
                }
                else
                {
                    if (not_released) // confirmation that you entered menu
                    {
                        leds_all_off();
                        for (int i = 0; i < LED_NUM - 1; i++)
                        {
                            led_fade_inout(i, 15, 1);
                            HAL_Delay(50);
                        }
                        HAL_Delay(200);
                    }
                    pass[1] = 1;
                    not_released = 0;
                    timer = HAL_GetTick();
                    if (pressed)
                    {
                        pressed = 0;
                        // delays this for the first press, otherwise will work as expected
                        // this is so it skips incrementing press_count 
                        released = 1; 
                        led_on_just_one(press_count, 32, 2);
                    }
                }

                if (HAL_GetTick() - timer_last_pressed > 2000)
                {
                    function = press_count + LED_TORCH_STATIC + 1;
                    break;
                }
                HAL_Delay(10);
            }
        }
        else
        {
            if (activity > MIN_ACTIVITY && !supress_flash)
            {
                int rand_count = random_in_range_exp(1, 3);
                int activity_led = activity;
                /* if (shake_count)
                {
                    activity_led /= 3;
                } */
                
                for (int flash = 0; flash < rand_count; flash++)
                {
                    if (percent_chance_bool(activity_led))
                    {
                        led_fade_inout(random_in_range(0, 5), random_in_range(40, MAX_PWM), random_in_range_exp(1, 3));
                        HAL_Delay(random_in_range(20, 200));
                    }
                }
            }
            if (supress_flash > 0)
                supress_flash--;
        }

        if (function > NO_FUNCTION)
        {
            if (function != LED_TORCH_STATIC)
            {
                leds_all_off();
            }
            switch (function)
            {
                case LED_TORCH_STATIC:
                    shake_count = 0;
                    led_fade(LED_W, MAX_PWM, 2);
                    int timer = HAL_GetTick();
                    while (shake_count < 20) 
                    {
                        update_shake(200);
                        if (shake_count > 0)
                        {
                            shake_count--;
                            timer = HAL_GetTick();
                        }
                        if (HAL_GetTick() - timer > 30000)
                            break;
                        //HAL_Delay(50);
                    }
                    led_fade(LED_W, 0, 1);
                    break;
                
                case EXIT_MENU:
                    for (int i = LED_NUM - 2; i >= 0; i--)
                    {
                        led_fade_inout(i, 15, 1);
                        HAL_Delay(50);
                    }
                    break;
                
                case LED_TORCH_SIGNAL:
                    while (button_state()) {}
                    while (!button_state()) 
                    {
                        led_fade(LED_W, MAX_PWM, 30);
                        HAL_Delay(50);
                        led_fade(LED_W, 0, 30);
                        HAL_Delay(100);
                    }
                    led_fade(LED_W, 0, 30);
                    break;

                case DISPLAY_TEMP:
                    HAL_ADC_Start(&hadc);
                    HAL_ADC_PollForConversion(&hadc, 100);
                    //uint32_t val = HAL_ADC_GetValue(&hadc);
                    break;

                case DISPLAY_BATTERY:
                    break;

                default:
                    display_error(10);
                    break;
            }
        }

        //led_fade_inout(LED_R, MAX_PWM, 4);
        //HAL_Delay(500);

        HAL_Delay(20);
        wait_for_leds();
        power_down();

        rtc_alarm = 0;
        shake_count = 0;
        press_count = 0;

        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    }
}
