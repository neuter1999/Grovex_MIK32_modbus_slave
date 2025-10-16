/*
	Код инициализации SPIFI (QSPI) интерфейса для работы в режиме XiP и включение 4-х битового режима. 

	Позаимствовано из репозитория https://gitflic.ru/project/rabidrabbit/mik32_amur_simple.git

	Оригинальный автор: Vladimir Zaytsev <sleepin.sun@yandex.ru>

	Переработано: Ruslan Zalata <rz@fabmicro.ru>

	Коментарий: переработка касается стилистики кода (TAB - наше всё!) и подьема ошибки в стиле Linux kernel.
 */

#include <mik32_memory_map.h>
#include <pad_config.h>
#include <gpio.h>

#include "mik32_hal_spifi.h"
#include "xprintf.h"

#define SPIFI_FLASH_SIZE		(8*1024*1024)
#define STATUS_REGISTER_QE (1<<1)

// ожидание завершения отработки команды контроллером SPIFI с указанным таймаутом в миллисекундах
int spifi_wait_cmd(uint32_t timeout_ms) {
	uint32_t count = 0;
	do {
		// проверяем бит INTRQ, который выставляется контроллером по завершении команды
		if ( 0 != (SPIFI_CONFIG->STAT & SPIFI_CONFIG_STAT_INTRQ_M) ) {
			return 0;
		}
	} while (count++ < timeout_ms*8000);
	return -1;
}

// ожидать завершения сброса контроллера SPIFI
int spifi_wait_reset(uint32_t timeout_ms) {
	uint32_t count = 0; 
	do {
		// проверяем бит RESET, который сбрасывается контроллером после завршения сброса
		if ((SPIFI_CONFIG->STAT & SPIFI_CONFIG_STAT_RESET_M) == 0) {
			return 0;
		}
	} while (count++ < timeout_ms*8000);
	return -1;
}

// ожидание завершения текущей операции записи/стирания, которую обрабатывает SPI FLASH
int spifi_wait_flash(uint32_t timeout_ms) {
	// ждём завершения операции (выполняемем Read Status Register-1 (05h), пока нулевой бит не станет равным нулю)
	SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
	SPIFI_CONFIG->CMD = (0 << SPIFI_CONFIG_CMD_POLL_INDEX_S) // номер отслеживаемого бита 0 (ноль), SR1.BUSY
			| (0 << SPIFI_CONFIG_CMD_POLL_REQUIRED_VALUE_S) // требуемое состояние отслеживаемого бита - 0 (ноль)
			| SPIFI_CONFIG_CMD_POLL_M // режим опроса (повторение чтения до получения требуемого состояни указанного бита)
			| (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
			| (0x05 << SPIFI_CONFIG_CMD_OPCODE_S)
			;
	// ждём завершения отработки команды - в данном случае команда завершится, когда контроллер SPIFI
	// прочитает регистр состояния с указанным состоянием бита
	if (spifi_wait_cmd(timeout_ms)) {
		return -1;
	}
	// прочитаем один байт из FIFO - это собственно содержимое регистра состояния
	SPIFI_CONFIG->DATA8;
	//
	return 0;
}


// Инициализация SPI FLASH
int spifi_init(void) {
	xprintf("%s: start\n", "spifi_init");

	// Включить тактирование SPIFI
	PM->CLK_AHB_SET = PM_CLOCK_AHB_SPIFI_M;

	// PORT2.0-PORT2.5 -> SPIFI (sck, cs, D0, D1, D2, D3)
	PAD_CONFIG->PORT_2_CFG = (PAD_CONFIG->PORT_2_CFG & ~(0x03FF))
			| PAD_CONFIG_PIN(0, 1)
			| PAD_CONFIG_PIN(1, 1)
			| PAD_CONFIG_PIN(2, 1)
			| PAD_CONFIG_PIN(3, 1)
			| PAD_CONFIG_PIN(4, 1)
			| PAD_CONFIG_PIN(5, 1)
			;
	PAD_CONFIG->PORT_2_DS &=	~0x03FF; // сброс Driving Strength на линиях SPIFI
	PAD_CONFIG->PORT_2_PUPD &= ~0x03FF; // сброс подтяжек на линиях SPIFI

	// Сброс SPIFI контроллера
	SPIFI_CONFIG->STAT = SPIFI_CONFIG_STAT_RESET_M;
	//
	if (spifi_wait_reset(50)) {
		xprintf("%s: reset timeout\n", "spifi_init");
		return -1;
	}

	SPIFI_CONFIG->CTRL |= SPIFI_CONFIG_CTRL_CSHIGH_M;
	// Настройка Fast Read Quad I/O на работу с передачей адреса
	// (если SPI FLASH был в однопроводном режиме, просто ничего не произойдёт)
	SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
	SPIFI_CONFIG->ADDR = 0;
	SPIFI_CONFIG->IDATA = 0x0; // содержимое первого dummy байта команды Fast Read Quad I/O (EBh)
	SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_DATALEN_S) // прочитаем один байт
			| (1 << SPIFI_CONFIG_MCMD_INTLEN_S) // один dummy байт 0x0
			| (3 << SPIFI_CONFIG_MCMD_FIELDFORM_S) // всё по четырём
			| (6 << SPIFI_CONFIG_MCMD_FRAMEFORM_S) // без кода команды, три байта адреса
			| (0xEB << SPIFI_CONFIG_MCMD_OPCODE_S)
			;
	// читаем один байт
	SPIFI_CONFIG->DATA8;
	// ожидаем завершения команды контроллером SPIFI
	if (spifi_wait_cmd(2)) {
		xprintf("%s: wait 0xEB timeout\n", "spifi_init");
		return -2;
	}

	// выполняем команду Disable QPI (FFh) (передача команд по одной линии)
	// (если SPI FLASH был в однопроводном режиме, просто ничего не произойдёт)
	SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
	SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
			| (3 << SPIFI_CONFIG_MCMD_FIELDFORM_S) // всё по четырём
			| (0xFF << SPIFI_CONFIG_CMD_OPCODE_S)
			;
	// ожидаем завершения команды контроллером SPIFI
	if (spifi_wait_cmd(2)) {
		xprintf("%s: cmd 0xFF timeout\n", "spifi_init");
		return -3;
	}

	// выполняем команду Read Status Register-2 (35h)
	SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
	SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_DATALEN_S) // читаем 1 байт
			| (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
			| (0x35 << SPIFI_CONFIG_CMD_OPCODE_S)
			;
	// выбираем прочитанное
	uint8_t v_sr2 = SPIFI_CONFIG->DATA8;
	// ожидаем завершения команды контроллером SPIFI
	if (spifi_wait_cmd(2)) {
		xprintf("%s: cmd 0x35 timeout\n", "spifi_init");
		return -4;
	}

	// если бит QUAD ENABLE не установлен
	if ((v_sr2 & STATUS_REGISTER_QE) == 0) {
		// будем его устанавливать
		// сначала выполняем команду Volatile Write Enable (50h) (запись бита только до отключения питания)
		SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
		SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
			| (0x50 << SPIFI_CONFIG_CMD_OPCODE_S)
			;
		// ожидаем завершения команды контроллером SPIFI
		if (spifi_wait_cmd(2)) {
			xprintf("%s: cmd 0x50 timeout\n", "spifi_init");
			return -5;
		}

		// потом команду Write Status Register-2 (31h)
		SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
		SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_DATALEN_S) // отправляем 1 байт
			| SPIFI_CONFIG_CMD_DOUT_M // выдача данных контроллером в SPI FLASH
			| (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды, без адреса
			| (0x31 << SPIFI_CONFIG_CMD_OPCODE_S)
			;
		// SR2 с взведённым битом Quad Enable
		SPIFI_CONFIG->DATA8 = v_sr2 | STATUS_REGISTER_QE;

		// ожидаем завершения команды контроллером SPIFI
		if (spifi_wait_cmd(2)) {
			xprintf("%s: cmd 0x31 timeout\n", "spifi_init");
			return -6;
		}

		// ожидаем завершения записи со стороны SPI FLASH
		if (spifi_wait_flash(16)) {
			xprintf("%s: wait write SREG2 timeout\n", "spifi_init");
			return -7;
		}
	}

	xprintf("%s: end\n", "spifi_init");
	return 0;
}

int spifi_enable_xip(void) {
	// сброс SPIFI
	SPIFI_CONFIG->STAT = SPIFI_CONFIG_STAT_RESET_M;
	//
	if (spifi_wait_reset(50)) {
		xprintf("%s: reset timeout\n", "spifi_enable_xip");
		return -1;
	}

	// отключение запросов к DMA в SPIFI
	SPIFI_CONFIG->CTRL &= ~SPIFI_CONFIG_CTRL_DMAEN_M;

	// отключение тактирования DMA и CRC32
	PM->CLK_AHB_CLEAR = PM_CLOCK_AHB_DMA_M | PM_CLOCK_AHB_CRC32_M;

	// выполняем команду Enable QPI (38h) (передача всего по 4 линиям)
	SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
	SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
										| (0x38 << SPIFI_CONFIG_CMD_OPCODE_S)
										;
	// ожидаем завершения отработки команды
	if (spifi_wait_cmd(2)) {
		xprintf("%s: cmd 0x38 timeout\n", "spifi_enable_xip");
		return -2;
	}

	// типа настройка Fast Read Quad I/O на работу с передачей исключительно адреса
	SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
	SPIFI_CONFIG->ADDR = 0;
	SPIFI_CONFIG->IDATA = 0x20; // содержимое первого dummy байта команды Fast Read Quad I/O (EBh)
	SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_DATALEN_S) // прочитаем один байт
		| (1 << SPIFI_CONFIG_MCMD_INTLEN_S) // один dummy байт 0x0 для режима QPI
		| (3 << SPIFI_CONFIG_MCMD_FIELDFORM_S) // всё по четырём
		| (4 << SPIFI_CONFIG_MCMD_FRAMEFORM_S) // код команды и три байта адреса
		| (0xEB << SPIFI_CONFIG_MCMD_OPCODE_S)
		;
	// читаем один байт
	SPIFI_CONFIG->DATA8;
	// ожидаем завершения отработки команды
	if (spifi_wait_cmd(2)) {
		xprintf("%s: cmd 0xEB timeout\n", "spifi_enable_xip");
		return -3;
	}

	// настроим SPIFI на работу "с памятью"
	SPIFI_CONFIG->CTRL = (SPIFI_CONFIG->CTRL & ~(SPIFI_CONFIG_CTRL_CSHIGH_M))
		| (0 << SPIFI_CONFIG_CTRL_CSHIGH_S) // 1 такт сигнала SCK между командами
		| SPIFI_CONFIG_CTRL_CACHE_EN_M // включение кэширования
		| SPIFI_CONFIG_CTRL_D_CACHE_DIS_M // отключение кэширования данных
		;
	SPIFI_CONFIG->ADDR = 0;
	SPIFI_CONFIG->IDATA = 0x20; // содержимое первого dummy байта команды Fast Read Quad I/O (EBh) (продолжаем использовать режим чтения без кода команды)
	SPIFI_CONFIG->CLIMIT = SPIFI_BASE_ADDRESS + SPIFI_FLASH_SIZE; // граница кэширования
	SPIFI_CONFIG->MCMD = (1 << SPIFI_CONFIG_MCMD_INTLEN_S) // один dummy байт 0x00 для режима QPI и только адреса
		| (3 << SPIFI_CONFIG_MCMD_FIELDFORM_S) // всё по четырём
		| (6 << SPIFI_CONFIG_MCMD_FRAMEFORM_S) // код команды, три байта адреса
		| (0xEB << SPIFI_CONFIG_MCMD_OPCODE_S)
		;

	return 0;
}

