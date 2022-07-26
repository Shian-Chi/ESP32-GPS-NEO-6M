/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
#include "string.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

uint8_t GPS_data[600];
uint8_t coordinate_size = 11;
uint8_t Latitude[11];  //Latitude 緯度, Data format: ddmm.mmmm;     dd.ddddd = dd + mm.mmmm/60
uint8_t Longitude[12]; //Longitude 經度, Data format: dddmm.mmmm;   ddd.ddddd = dd + mm.mmmm/60
uint8_t N_S[1], E_W[1];

uint16_t *latitude = Latitude;
uint16_t *longitude = Longitude;
uint16_t *n_s = &N_S[0];
uint16_t *e_w = &E_W[0];

bool GPS_State(uint16_t data_point);
void get_coordinate(void);
uint16_t point = 0;

void init()
{
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

int sendData(const char *logName, const char *data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void tx_task()
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1)
    {
        sendData(TX_TASK_TAG, "Hello world");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

static void rx_task()
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    while (1)
    {
        const int rxBytes = uart_read_bytes(UART_NUM_1, GPS_data, RX_BUF_SIZE, 500 / portTICK_RATE_MS);
        if (rxBytes > 0)
        {
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, GPS_data);
            if (GPS_State(point))
            {
                get_coordinate();
            }
        }
        memset(GPS_data, 0, sizeof(GPS_data));
    }
}

bool GPS_State(uint16_t data_point)
{   
    bool flag = false;
    for (uint16_t i = 0; i <= 600; i++)
    {
        if (GPS_data[i] == '$' && GPS_data[i + 3] == 'R')
        {
            for (uint8_t j = i + 3; j <= 30; j++)
            {
                if (GPS_data[j] == 'A') //data correct
                {
                    point = j;
                    flag = true;
                    break;
                }
                else if (GPS_data[j] == 'V')
                {
                    point = j;
                    flag = false;
                }
            }
        }
        /*if (flag == false)
            printf("false");
        else
            printf("true");
        */
    }

    return flag;
}

void get_coordinate(void) 
{
    uint8_t count = 0;
    memset(Latitude, 0, sizeof(Latitude));
    memset(Longitude, 0, sizeof(Longitude));
    for (uint8_t i = point; i <= 100; i++)
    {
        if (GPS_data[i] == ',')
            count++;

        switch (count)
        {
        case 1:
        {
            if (GPS_data[i + 1] == '-')
                memcpy(Latitude, &GPS_data[i + 1], 11);
            else
                memcpy(Latitude, &GPS_data[i + 1], 10);
            count++;
            printf("Latitude: %s\n", (uint8_t *)*latitude);

            break;
        }
        case 3:
        {
            N_S[0] = GPS_data[i + 1];
            count++;
            printf("N_S: %s\n", (uint8_t *)*n_s);

            break;
        }
        case 5:
        {
            if (GPS_data[i + 1] == '-')
                memcpy(Longitude, &GPS_data[i + 1], 12);
            else
                memcpy(Longitude, &GPS_data[i + 1], 11);
            count++;
            printf("Longitude: %s\n", (uint8_t *)*longitude);

            break;
        }
        case 7:
        {
            E_W[0] = GPS_data[i + 1];
            count++;
            printf("E_W: %s\n", (uint8_t *)*e_w);

            break;
        }
        }
    }
}

void app_main()
{
    init();
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES, NULL);
    //xTaskCreate(tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
}
