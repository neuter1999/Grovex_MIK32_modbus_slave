#include "stdint.h"
#include "xprintf.h"
#include "modbus.h"

//Функции для формирования контролной суммы и разбиения регистров на байты

uint16_t chek_sum(uint8_t* RX_Mster_Data, uint8_t length_) //Функция проверки контрольной суммы полученной от мастера
{
        register uint16_t j; //Номер бита, отдлельного байта
        register uint16_t reg_crc = 0xFFFF; //Регистр именяемый в процессе рачета
        while (length_--)
        {
                reg_crc ^= *RX_Mster_Data++;
                for(j=0;j<8;j++)
                {
                        if(reg_crc & 0x01)
                        {
                                reg_crc = (reg_crc >> 1) ^ 0xA001;
                        }
                        else
                        {
                                reg_crc = reg_crc >> 1;
                        }
                }

        }
        return reg_crc;
}

void byte_to_int(uint16_t* input, uint8_t low_byte, uint8_t high_byte)//Функция сложения двух байт в одно int число
{
        *input = high_byte;
        *input = (*input << 8);
        *input |= low_byte;
}

void int_to_byte(uint16_t* input, uint8_t* low_byte, uint8_t* high_byte)//Функция разделения двух байт
{
        *low_byte = *input;
        *high_byte = *input>>8;
}

void func_fl_to_reg(float in_func_fl, uint8_t start_data, uint16_t data[]) //Функция для разбиения флоата на 2 регистра, для передачи по Модбас
{
        union fl_to_reg fl;
        fl.in_fl = in_func_fl;
        data[start_data] = fl.out_reg[0];
        data[start_data + 1] = fl.out_reg[1];
}

//Функции кольцевого буфера

void init(sCircularBuffer *apArray) //Функция инициализации кольцевого буфера
{
	apArray->readIndex  = 0;
  	apArray->writeIndex = 0;
  	apArray->isEmpty    = 1;
  	apArray->isFull     = 0;
}

uint16_t put(sCircularBuffer *apArray,  uint8_t aValue) //Функция добавления элемента в кольцевой буфер
{
  	if(apArray->isFull)
    		return -1;
  	if(apArray->writeIndex >= BUFFER_SIZE)
    		apArray->writeIndex = 0;
  	if(apArray->isEmpty)
  	{
    		apArray->isEmpty = 0;
    		apArray->data[apArray->writeIndex++] = aValue;

    		if (apArray->writeIndex == apArray->readIndex)
      			apArray->isFull  = 1;
    	return 1;
  	}
  	apArray->data[apArray->writeIndex++] = aValue;
  	if (apArray->writeIndex == apArray->readIndex)
    		apArray->isFull  = 1;
  	return 1;
}

uint8_t get(sCircularBuffer *apArray) //Функция получения элемента с кольцевого буфера
{
  	if(apArray->isEmpty)
    		return -1;

  	apArray->isFull = 0;

  	if(apArray->readIndex >= BUFFER_SIZE)
    		apArray->readIndex = 0;

  	uint8_t res = apArray->data[apArray->readIndex++];

  	if(apArray->readIndex == apArray->writeIndex)
    		apArray->isEmpty = 1;

  	return  res;
}

void clear(sCircularBuffer *apArray) //Функция очистки кольцевого буфера
{
  	apArray->isEmpty    = 1;
  	apArray->isFull     = 0;
  	apArray->writeIndex = 0;
  	apArray->readIndex  = 0;
}

uint16_t isEmpty(sCircularBuffer *apArray) //Функция проверки кольцевого буфера , возвращает единицу, если буфер пустой
{
  	return apArray->isEmpty;
}

uint16_t isFull(sCircularBuffer *apArray) //Функция проверки кольцевого буфера, возвращает единицу, если буфер полный 
{
  	return apArray->isFull;
}

//Функции для использования МК в качестве modbus slave (Функции 03 04 06 10)

uint16_t mb_slave_error(uint8_t tx_mb_data[], uint8_t slave_id, uint8_t function, uint8_t type_error)
//Формирование фрейма ошибок (Для уницифицированных ошибок)
//1) Массив, который будет отправлятся в порт
//2) Слейв айди
//3) Функция 
//4) Тип ошибки 
//Функция возвращает величину отправляемого пакета
{
	uint16_t num_byte_in_frame = 5;
	uint16_t CRC_res = 0;
	tx_mb_data[0] = slave_id;
	tx_mb_data[1] = function + 128;
	tx_mb_data[2] = type_error;
        CRC_res = chek_sum(tx_mb_data, num_byte_in_frame - 2);
        int_to_byte(&CRC_res, &tx_mb_data[num_byte_in_frame-2], &tx_mb_data[num_byte_in_frame-1]);
	return num_byte_in_frame;
}

uint16_t mb_slave_read(uint8_t rx_mb_data[], uint8_t tx_mb_data[], uint8_t num_byte_rx, uint8_t slave_id, uint8_t function, uint16_t data[], uint8_t max_data_byte, uint8_t data_start)
//Формирование фрейма ответа на запрос от мастера на чтение
//1) Принятый запрос
//2) Фрейм, который будет формироваться
//3) Количество байт в запросе
//4) Слейв айди
//5) Функция
//6) Данные, котоыре будут считаны
//7) Количество байт доступных для чтения
//8) Стартовый байт с которого доступно чтение
//Функция возвращает величину отправляемого пакета
{
	uint16_t num_byte_in_frame = 0;
	uint16_t num_data_byte = 0;
	uint16_t data_start_read = 0;
        uint16_t CRC_res = 0;
	if(rx_mb_data[0] == slave_id)
	{
           	CRC_res = chek_sum(rx_mb_data, num_byte_rx - 2);//Расчёт контрольной суммы запроса
             	if((rx_mb_data[num_byte_rx-1] == ((CRC_res >>8) & 0x00FF)) && (rx_mb_data[num_byte_rx-2] == (CRC_res & 0x00FF)) )
             	{
			if(rx_mb_data[1] == function)
			{
				byte_to_int(&num_data_byte, rx_mb_data[5], rx_mb_data[4]);
				byte_to_int(&data_start_read, rx_mb_data[3], rx_mb_data[2]);

				if(num_data_byte + data_start_read > max_data_byte)
					num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x03);
                                if((data_start_read > max_data_byte - 1) || (data_start_read < data_start))
                                       	num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x02);

				if((num_data_byte + data_start_read <= max_data_byte) && (data_start_read <= (max_data_byte - 1)) && (data_start_read >= data_start))
                                {
	                                tx_mb_data[0] = slave_id;
                                	tx_mb_data[1] = function;
                                        tx_mb_data[2] = 2*(uint8_t)num_data_byte;
                                        num_byte_in_frame = 5 + num_data_byte*2;
                                        volatile uint16_t j = 0;
                                        for(volatile uint16_t i = 3; i < (num_byte_in_frame - 2); i+=2)
                                        {
                                        	int_to_byte(&data[data_start_read+j], &tx_mb_data[i+1], &tx_mb_data[i]);
                                                j++;
					}
                                        CRC_res = chek_sum(tx_mb_data, (num_byte_in_frame - 2));
                                        int_to_byte(&CRC_res, &tx_mb_data[num_byte_in_frame - 2], &tx_mb_data[num_byte_in_frame - 1]);
				}
			}
			else
				num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x01);
		}
		else
			num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x0C);//Не совпадает контрольная сумма
	}

	return num_byte_in_frame;
}

uint16_t mb_slave_write_single(uint8_t rx_mb_data[], uint8_t tx_mb_data[], uint8_t num_byte_rx, uint8_t slave_id, uint8_t function, uint16_t data[], uint8_t max_data_byte, uint8_t data_start)
//Формирование фрейма для записи от мастера функция 06  и 05
//Нужно указать:
//1) Принятый запрос
//2) Фрейм который будет сформирован
//3) Количество байт в запросе
//4) Слейв айди
//5) Функция
//6) Регистр в которые будут записаны полученные данные
//7) Количество байт в массиве, в котоырй будем производится запись
//8) Стартовый адрес массива для записи
//Функция возвращает величину отправляемого пакета
{
	uint16_t num_byte_in_frame = 0;
	uint16_t num_data_byte = 0;
	uint16_t data_start_write = 0;
        uint16_t CRC_res = 0;
	if(rx_mb_data[0] == slave_id)
	{
           	CRC_res = chek_sum(rx_mb_data, num_byte_rx - 2);//Расчёт контрольной суммы запроса
             	if((rx_mb_data[num_byte_rx-1] == ((CRC_res >>8) & 0x00FF)) && (rx_mb_data[num_byte_rx-2] == (CRC_res & 0x00FF)) )
             	{
			if(rx_mb_data[1] == function)
			{
				byte_to_int(&data_start_write, rx_mb_data[3], rx_mb_data[2]);
                                if((data_start_write > max_data_byte - 1) || (data_start_write < data_start))
                                       	num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x02);

				if((data_start_write <= (max_data_byte - 1)) && (data_start_write >= data_start))
                                {
                                        num_byte_in_frame = 8;
					for(volatile uint16_t i = 0; i < num_byte_in_frame; i++)
						tx_mb_data[i] = rx_mb_data[i];
					byte_to_int(&data[data_start_write], rx_mb_data[5], rx_mb_data[4]);
                                        CRC_res = chek_sum(tx_mb_data, (num_byte_in_frame - 2));
                                        int_to_byte(&CRC_res, &tx_mb_data[num_byte_in_frame - 2], &tx_mb_data[num_byte_in_frame - 1]);
				}
			}
			else
				num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x01);
		}
		else
			num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x0C);//Не совпадает контрольная сумма
	}

	return num_byte_in_frame;
}

uint16_t mb_slave_write_multi(uint8_t rx_mb_data[], uint8_t tx_mb_data[], uint8_t num_byte_rx, uint8_t slave_id, uint8_t function, uint16_t data[], uint8_t max_data_byte, uint8_t data_start)
//Формирование фрейма ответа на запрос от мастера на чтение
//1) Принятый запрос
//2) Фрейм, который будет формироваться
//3) Количество байт в запросе
//4) Слейв айди
//5) Функция
//6) Данные, котоыре будут считаны
//7) Количество байт доступных для чтения
//8) Стартовый байт с которого доступно чтение
//Функция возвращает величину отправляемого пакета
{
	uint16_t num_byte_in_frame = 0;
	uint16_t  num_data_byte = 0;
	uint16_t  data_start_write = 0;
	uint16_t CRC_res = 0;
	if(rx_mb_data[0] == slave_id)
	{
           	CRC_res = chek_sum(rx_mb_data, num_byte_rx - 2);//Расчёт контрольной суммы запроса
             	if((rx_mb_data[num_byte_rx-1] == ((CRC_res >>8) & 0x00FF)) && (rx_mb_data[num_byte_rx-2] == (CRC_res & 0x00FF)) )
             	{
			if(rx_mb_data[1] == function)
			{
				byte_to_int(&num_data_byte, rx_mb_data[5], rx_mb_data[4]);
				byte_to_int(&data_start_write, rx_mb_data[3], rx_mb_data[2]);
                               
				if(num_data_byte + data_start_write > max_data_byte)
                                        num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x03);
                                if((data_start_write > max_data_byte - 1) || (data_start_write < data_start))
                                        num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x02);

                                if((num_data_byte + data_start_write <= max_data_byte) && (data_start_write <= (max_data_byte - 1)) && (data_start_write >= data_start))
                                {
                                	num_byte_in_frame = 8;
					for(volatile uint16_t i = 0; i < 6; i++)
						tx_mb_data[i] = rx_mb_data[i];
					
					volatile uint16_t j = data_start_write;	
					for(volatile uint16_t k = 0; k < 2*num_data_byte; k+=2)
					{
						byte_to_int(&data[j], rx_mb_data[8+k], rx_mb_data[7+k]);
						j++;
					}

                                        CRC_res = chek_sum(tx_mb_data, (num_byte_in_frame - 2));
                                        int_to_byte(&CRC_res, &tx_mb_data[num_byte_in_frame - 2], &tx_mb_data[num_byte_in_frame - 1]);
				}
			}
			else
				num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x01);
		}
		else
			num_byte_in_frame = mb_slave_error(tx_mb_data, slave_id, function, 0x0C);//Не совпадает контрольная сумма
	}

	return num_byte_in_frame;
}

//Функции для использования МК в качестве modbus master (Функции 03 04 06 10)

uint16_t mb_master_read(uint8_t tx_mb_data[], uint8_t slave_id, uint8_t function,  uint16_t first_reg, uint16_t num_reg)
//Формирование фрейма для чтения от мастера
//Нужно указать: 
//1) Массив, который будет отправлятся в порт 
//2) слейв айди
//3) Функция  
//4) Первый регистр
//5) Количество регистров
//Функция возвращает величину отправляемого пакета
{
	uint16_t num_byte_in_frame = 8;
	uint16_t CRC_res = 0;
      	tx_mb_data[0] = slave_id;
       	tx_mb_data[1] = function;
      	int_to_byte(&first_reg, &tx_mb_data[3], &tx_mb_data[2]);
       	int_to_byte(&num_reg, &tx_mb_data[5], &tx_mb_data[4]);
       	CRC_res = chek_sum(tx_mb_data, num_byte_in_frame - 2);
	int_to_byte(&CRC_res, &tx_mb_data[num_byte_in_frame-2], &tx_mb_data[num_byte_in_frame-1]);
	return num_byte_in_frame;
}

uint16_t mb_master_write_multi(uint8_t tx_mb_data[], uint8_t slave_id, uint8_t function, uint16_t first_reg, uint16_t num_reg, uint8_t data[])
//Формирование фрейма для записи от мастера функция 10 и 0F
//Нужно указать:
//1) Фрейм, который будет отправлятся в порт
//2) Слейв айди 
//3) Функция
//4) Первый регистр 
//5) Количество регистров
//6) Пакет байт который будет записан
//Функция возвращает величину отправляемого пакета
{
	volatile uint16_t i = 0;
	uint16_t CRC_res = 0;
	uint16_t num_byte_in_frame = 9 + (num_reg * 2);
        tx_mb_data[0] = slave_id;
        tx_mb_data[1] = function;
        int_to_byte(&first_reg, &tx_mb_data[3], &tx_mb_data[2]);
        int_to_byte(&num_reg, &tx_mb_data[5], &tx_mb_data[4]);
	tx_mb_data[6] = num_reg*2;
	i = 0;
	for(volatile uint16_t i = 0; i < tx_mb_data[6]; i++)
		tx_mb_data[7+i] = data[i];
        CRC_res = chek_sum(tx_mb_data, num_byte_in_frame - 2);
        int_to_byte(&CRC_res, &tx_mb_data[num_byte_in_frame-2], &tx_mb_data[num_byte_in_frame-1]);
	return num_byte_in_frame;
}

uint16_t mb_master_write_single(uint8_t tx_mb_data[], uint8_t slave_id, uint8_t function, uint16_t first_reg, uint16_t data)
//Формирование фрейма для записи от мастера функция 06  и 05
//Нужно указать:
//1) Фрейм, который будет отправлятся в порт
//2) Слейв айди 
//3) Функция
//4) Первый регистр   
//5) Количество регистров
//6) 16 битное слово для записи в регистр
//Функция возвращает величину отправляемого пакета
{
        uint16_t num_byte_in_frame = 8;
        uint16_t CRC_res = 0;
        tx_mb_data[0] = slave_id;
        tx_mb_data[1] = function;
        int_to_byte(&first_reg, &tx_mb_data[3], &tx_mb_data[2]);
        int_to_byte(&data, &tx_mb_data[5], &tx_mb_data[4]);
        CRC_res = chek_sum(tx_mb_data, num_byte_in_frame - 2);
        int_to_byte(&CRC_res, &tx_mb_data[num_byte_in_frame-2], &tx_mb_data[num_byte_in_frame-1]);
        return num_byte_in_frame;

}

uint8_t mb_master_read_rx_error(uint8_t rx_mb_data[], uint8_t slave_id)
//Обработка ошибок ответа слейва
//Нужно указать:
//1) Фрейм, который будет принят
//2) Слейв айди 
//Функция возвращает номер ошибки Modbus (Уницифированный)
{
	uint8_t error = 0;

	if(rx_mb_data[0] == slave_id)
	{
		if(rx_mb_data[1] >= 128)
			error = rx_mb_data[2];
	}
	return error;
}

uint8_t mb_master_read_rx(uint8_t rx_mb_data[], uint8_t data[], uint8_t slave_id, uint8_t function, uint16_t num_reg)
//Обработка ответа  чтения
//Нужно указать:
//1) Фрейм, который будет принят
//2) Массив в который будут складываться байты
//3) Слейв айди
//4) Функция
//5) Количество запрошенных регистров
//Функция возвращает номер ошибки Modbus (Уницифированный)
{
	uint8_t error = 0;
	uint8_t num_byte = 0;
        uint16_t CRC_res = 0;

	error = mb_master_read_rx_error(rx_mb_data, slave_id);

	if(error == 0)
	{
		if(rx_mb_data[0] == slave_id)
		{
			if(rx_mb_data[1] == function)
			{
				num_byte = num_reg*2 + 5;
        			CRC_res = chek_sum(rx_mb_data, num_byte - 2);
        			if((rx_mb_data[num_byte-1] == ((CRC_res >>8) & 0x00FF)) && (rx_mb_data[num_byte-2] == (CRC_res & 0x00FF)) )
                                {
					for(volatile uint16_t i = 0; i < (2*num_reg) ; i++)
						data[i] = rx_mb_data[i + 3];
				}
				else
					error = 0x0C; //Контрольная сумма не верна
			}
		}
	}
	return error;
}

uint8_t mb_master_write_single_rx(uint8_t rx_mb_data[], uint8_t tx_mb_data[])
//Обработка ответа при сингл записях
//1) Фрейм, который будет принят
//2) Фрейм запроса
//Функция возвращает номер ошибки Modbus (Уницифированный)
{
	uint8_t error = 0;

	error = mb_master_read_rx_error(rx_mb_data, tx_mb_data[0]);
	
	if (error == 0)
	{
		for(volatile uint16_t i = 0; i < 8; i++)
		{
			if(rx_mb_data[i] != tx_mb_data[i])
				error = 0x0D;//Не унифицированная ошибка
		}
	}
	return error;
}

uint8_t mb_master_write_multi_rx(uint8_t rx_mb_data[], uint8_t tx_mb_data[])
//Обработка ответа при мульти записи
//1) Фрейм, который будет принят
//2) Фрейм запроса
//Функция возвращает номер ошибки Modbus (Уницифированный)
{
	uint8_t error = 0;
	uint16_t CRC_res = 0;

        error = mb_master_read_rx_error(rx_mb_data, tx_mb_data[0]);

        if (error == 0)
	{ 
		CRC_res = chek_sum(rx_mb_data, 6);
		if((rx_mb_data[7] == ((CRC_res >>8) & 0x00FF)) && (rx_mb_data[6] == (CRC_res & 0x00FF)) )
        	{
                	for(volatile uint16_t i = 0; i < 6; i++)
                	{
                        	if(rx_mb_data[i] != tx_mb_data[i])
                                	error = 0x0D;//Не унифицированная ошибка
                	}
        	}
		else
			error = 0x0C;//Контрольная сумма не верна
	}
        return error;

}
