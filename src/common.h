#ifndef __COMMON_H__
#define __COMMON_H__

#define	MODEM_RX_BUF_SIZE	128

#define	PAD_CFG_PUPD(Port)		PORT_ ## Port ## _PUPD
#define	PAD_CFG_MODE(Port)		PORT_ ## Port ## _CFG
#define	PAD_GPIO(Port)			GPIO_ ## Port

// Макро для установки режимов подтяжки оптимальным способом (генерирует короткий код)
#define	PAD_PUPD(Port, Pin, UpDown) {\
	uint32_t x = (PAD_CONFIG->PAD_CFG_PUPD(Port));\
	x &= ~(0b11 << (2 * Pin));\
	x |= (UpDown << (2 * Pin));\
	(PAD_CONFIG->PAD_CFG_PUPD(Port)) = x;\
}

// Макро для установки режимов работы (GPIO, Func) 
#define	PAD_MODE(Port, Pin, Mode) {\
	uint32_t x = (PAD_CONFIG->PAD_CFG_MODE(Port));\
	x &= ~(0b11 << (2 * Pin));\
	x |= (Mode << (2 * Pin));\
	(PAD_CONFIG->PAD_CFG_MODE(Port)) = x;\
}

// Макро для сброса режимов работы PAD-а в дефолтное состояние
#define	PAD_MODE_CLEAR(Port, Pin) {\
	uint32_t x = (PAD_CONFIG->PAD_CFG_MODE(Port));\
	x &= ~(0b11 << (2 * Pin));\
	(PAD_CONFIG->PAD_CFG_MODE(Port)) = x;\
}

// Макро для настройки PAD-а в режим GPIO input 
#define	PAD_CFG_IN(Port, Pin) {\
	PAD_MODE_CLEAR(Port, Pin);\
	PAD_GPIO(Port)->DIRECTION_IN |= (1 << Pin);\
}	

// Макро для настройки PAD-а в режим GPIO output
#define	PAD_CFG_OUT(Port, Pin) {\
	PAD_MODE_CLEAR(Port, Pin);\
	PAD_GPIO(Port)->DIRECTION_OUT |= (1 << Pin);\
}	


// Макро установки выходного GPIO в лог "1"
#ifdef GPIO_SET
#undef GPIO_SET
#endif
#define	GPIO_SET(Port, Pin) PAD_GPIO(Port)->OUTPUT |= (1 << Pin)

// Макро установки выходного GPIO в лог "0"
#ifdef GPIO_CLEAR
#undef GPIO_CLEAR
#endif
#define	GPIO_CLEAR(Port, Pin) PAD_GPIO(Port)->OUTPUT &= ~(1 << Pin)

// Макро инвертирования выходного GPIO
#ifdef GPIO_INVERT
#undef GPIO_INVERT
#endif
#define	GPIO_INVERT(Port, Pin) PAD_GPIO(Port)->OUTPUT ^= (1 << Pin)

// Макро чтение состояния входного GPIO
#ifdef GPIO_STATE
#undef GPIO_STATE
#endif
#define	GPIO_STATE(Port, Pin) (PAD_GPIO(Port)->STATE & (1 << Pin))

static void DelayMs(int delay)
{
	uint64_t begin_tick = (uint64_t)SCR1_TIMER->MTIMEH << 32 | SCR1_TIMER->MTIME;

	while(1) {
		uint64_t cur_tick = (uint64_t)SCR1_TIMER->MTIMEH << 32 | SCR1_TIMER->MTIME;
		if(cur_tick - begin_tick >= delay * 1000)
			break;
	}
}

#endif // __COMMON_H__

