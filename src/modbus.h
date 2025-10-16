//Функции для формирования контролной суммы и разбиения регистров на байты

uint16_t chek_sum(uint8_t* RX_Mster_Data, uint8_t count_res_);//Функция проверки контрольной суммы полученной от мастера
void byte_to_int(uint16_t* input, uint8_t low_byte, uint8_t high_byte);//Функция сложения двух байт в одно int число
void int_to_byte(uint16_t* input, uint8_t* low_byte, uint8_t* high_byte);//Функция разделения двух байт

union fl_to_reg
{
        float in_fl;
        uint16_t out_reg[2];
};

void func_fl_to_reg(float in_func_fl, uint8_t start_data, uint16_t data[]); //Функция для разбиения флоата на 2 регистра, для передачи по Модбас

//Кольцевой буфер

//Переменные:
#define BUFFER_SIZE 512

typedef struct
{
	uint16_t readIndex;             //Индекс считываемого компонента
  	uint16_t writeIndex;            //Индекс записываемого компонента
  	uint16_t isEmpty;               //Буфер пустой
  	uint16_t isFull;                //Буфер полный 
  	uint8_t data[BUFFER_SIZE];      //Сам бфер с данными
}sCircularBuffer;

//Функции:
void init(sCircularBuffer *apArray);                   //Функция инициализации кольцевого буфера
uint16_t put(sCircularBuffer *apArray, uint8_t aValue);//Функция добавления элемента в кольцевой буфер
uint8_t get(sCircularBuffer *apArray);                 //Функция получения элемента с кольцевого буфера
void clear(sCircularBuffer *apArray);                  //Функция очистки кольцевого буфера
uint16_t isEmpty(sCircularBuffer *apArray);            //Функция проверки кольцевого буфера , возвращает единицу, если буфер пустой
uint16_t isFull(sCircularBuffer *apArray);             //Функция проверки кольцевого буфера, возвращает единицу, если буфер полный 

//Функции для использования МК в качестве modbus slave (Функции 03 04 06 10)

//Обработка ответов
uint16_t mb_slave_error(uint8_t tx_mb_data[], uint8_t slave_id, uint8_t function, uint8_t type_error);//Функция для генерации ошибок для слейва
uint16_t mb_slave_read(uint8_t rx_mb_data[], uint8_t tx_mb_data[], uint8_t num_byte_rx, uint8_t slave_id, uint8_t function, uint16_t data[], uint8_t max_data_byte, uint8_t data_start);//Функция для формирования ответов чтения
uint16_t mb_slave_write_single(uint8_t rx_mb_data[], uint8_t tx_mb_data[], uint8_t num_byte_rx, uint8_t slave_id, uint8_t function, uint16_t data[], uint8_t max_data_byte, uint8_t data_start); //Фукнция формирования ответа на синг запись
uint16_t mb_slave_write_multi(uint8_t rx_mb_data[], uint8_t tx_mb_data[], uint8_t num_byte_rx, uint8_t slave_id, uint8_t function, uint16_t data[], uint8_t max_data_byte, uint8_t data_start); //Функция формирования ответа на мульти запись

//Функции для использования МК в качестве modbus master (Функции 03 04 06 10)

//Запросы
uint16_t mb_master_read(uint8_t tx_mb_data[], uint8_t slave_id, uint8_t function,  uint16_t first_reg, uint16_t num_reg);//Функции чтения слейва
uint16_t mb_master_write_multi(uint8_t tx_mb_data[], uint8_t slave_id, uint8_t function, uint16_t first_reg, uint16_t num_reg, uint8_t data[]);//Функции мульти записей
uint16_t mb_master_write_single(uint8_t tx_mb_data[], uint8_t slave_id, uint8_t function, uint16_t first_reg, uint16_t data);//Функции сингл записей

//Обработка ответов
uint8_t mb_master_read_rx_error(uint8_t rx_mb_data[], uint8_t slave_id);//Функция обработки ошибок от слейва
uint8_t mb_master_read_rx(uint8_t rx_mb_data[], uint8_t data[], uint8_t slave_id, uint8_t function, uint16_t num_reg);//Функция обработки ответа чтения
uint8_t mb_master_write_single_rx(uint8_t rx_mb_data[], uint8_t tx_mb_data[]);//Функция обработки ответа от синг записи
uint8_t mb_master_write_multi_rx(uint8_t rx_mb_data[], uint8_t tx_mb_data[]);//Функция обработки ответа от мульти записи
