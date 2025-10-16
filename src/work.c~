#include <mik32_memory_map.h>
#include <pad_config.h>
#include <gpio.h>

#include "xprintf.h"
#include "scr1_timer.h"
#include "common.h"
#include "hardware.h"

// Явно указать на размещение переменной (указателя) и её данных в NOR Flash (секция .spidi.rodata)
__attribute__((section(".spifi.rodata")))
	const char test_str_in_flash[]
		__attribute__((section(".spifi.rodata"))) = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXX\r\n";


void ledBlink(void)
{
	GPIO_INVERT(LED_GRN_PORT, LED_GRN_PIN);
}

void ledButton(void)
{
	// Проверка состояния состояния кнопки 
	if (GPIO_STATE(SW1_ADDRx_PORT, SW1_ADDR0_PIN)) {
		GPIO_SET(LED_RED_PORT, LED_RED_PIN);
	} else {
		GPIO_CLEAR(LED_RED_PORT, LED_RED_PIN);
	}
}

__attribute__((used, section(".spifi.text")))
void work(void) {
	static int counter = 0;

	ledBlink(); /* Инвертируем светодиод LED_GRN */

	ledButton(); /* Зажигаем светодиод LED_RED при нажатой кнопке */

	xprintf("Hello, world! Counter = %d, GPIO_1->STATE = 0x%08X\r\n", counter++, GPIO_1->STATE);
	
	xprintf(test_str_in_flash);
	
	for (volatile int i = 0; i < 100000; i++);
}

