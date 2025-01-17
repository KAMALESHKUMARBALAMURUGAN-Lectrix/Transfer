/*
* SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: CC0-1.0
*/
 
/*
* The following example demonstrates a master node in a TWAI network. The master
* node is responsible for initiating and stopping the transfer of data messages.
* The example will execute multiple iterations, with each iteration the master
* node will do the following:
* 1) Start the TWAI driver
* 2) Repeatedly send ping messages until a ping response from slave is received
* 3) Send start command to slave and receive data messages from slave
* 4) Send stop command to slave and wait for stop response from slave
* 5) Stop the TWAI driver
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/twai.h"
 
#include "esp_timer.h"
 
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "cJSON.h"
 
// #include <ds18b20.h> // temperature sensor
 
/* --------------------- Definitions and static variables ------------------ */
// Example Configuration
#define PING_PERIOD_MS 250
#define NO_OF_DATA_MSGS 50
#define NO_OF_ITERS 3
#define ITER_DELAY_MS 500
#define RX_TASK_PRIO 9
#define TX_TASK_PRIO 11
#define CTRL_TSK_PRIO 8
#define TX_GPIO_NUM CONFIG_EXAMPLE_TX_GPIO_NUM
#define RX_GPIO_NUM CONFIG_EXAMPLE_RX_GPIO_NUM
#define EXAMPLE_TAG "TWAI Master"
 
#define ID_MASTER_STOP_CMD 0x0A0
#define ID_MASTER_START_CMD 0x0A1
#define ID_MASTER_PING 0x0A2
#define ID_SLAVE_STOP_RESP 0x0B0
#define ID_SLAVE_DATA 0x0B1
#define ID_SLAVE_PING_RESP 0x0B2
 
#define ID_BATTERY_3_VI 0x1725
#define ID_BATTERY_2_VI 0x1bc5
#define ID_BATTERY_1_VI 0x1f05
 
#define ID_BATTERY_3_SOC 0x1727
#define ID_BATTERY_2_SOC 0x1bc7
#define ID_BATTERY_1_SOC 0x1f07
 
 
#define ID_LX_BATTERY_VI 0x6
#define ID_LX_BATTERY_T 0xa
#define ID_LX_BATTERY_SOC 0x8
#define ID_LX_BATTERY_PROT 0x9
 
 
 
 
#define ID_MOTOR_RPM 0x14520902
#define ID_MOTOR_TEMP 0x233
#define ID_MOTOR_CURR_VOLT 0x32
 
//////////  temperature sensors ////////////////////////////////////////                           Temperature sensor
 
#define TEMP_BUS 23
 
static uint8_t tempSensor1[] = {0x28, 0xff, 0x64, 0x02, 0xee, 0x5f, 0x28, 0xfc};
static uint8_t tempSensor2[] = {0x28, 0xff, 0x64, 0x02, 0xe9, 0x4c, 0x6b, 0x9b};
static uint8_t tempSensor3[] = {0x28, 0xff, 0x64, 0x02, 0xe9, 0xda, 0x63, 0x30};
static uint8_t tempSensor4[] = {0x28, 0xff, 0x64, 0x02, 0xe9, 0xda, 0x13, 0xc8};
 
static float temp_sense1 = 0;
static float temp_sense2 = 0;
static float temp_sense3 = 0;
float temp_sense4 = 0;
 
char T_sense1[32];
char T_sense2[32];
char T_sense3[32];
char T_sense4[32];
 
//////////////////////////////////////////////////////////////////////////
 
static float Voltage_1 = 0;
static float Current_1 = 0;
static float SOC_1 = 0;
static float SOH_1 = 0;
 
static int Voltage_2 = 0;
static int Current_2 = 0;
static float SOC_2 = 0;
static float SOH_2 = 0;
 
static float Voltage_3 = 0;
static float Current_3 = 0;
static int SOC_3 = 0;
static int SOH_3 = 0;
static float SOCAh = 0;
 
// static uint16_t iDs[ 2 ] = {0x1f00,0x1bc0,0x1720};
 
static uint16_t iDs[2] = {0x1f00, 0x1bc0};
 
static int M_RPM = 0;
static int M_CONT_TEMP = 0;
static int M_MOT_TEMP = 0;
static int M_THROTTLE = 0;
 
static int M_AC_CURRENT = 0;
static int M_AC_VOLTAGE = 0;
static int M_DC_CURRENT = 0;
static int M_DC_VOLATGE = 0;
 
static int S_DC_CURRENT = 0;
static int S_AC_CURRENT1 = 0;
static int S_AC_CURRENT2 = 0;
 
static float TRIP_1 = 0;
static float TRIP1 = 0;
 
static int t_stamp = 0;
int adc_value = 0;
int adc_value1 = 0;
int adc_value2 = 0;
 
uint32_t RPM;
 
char motor_err[32];
 
char batt_err[32];
char batt_temp[32];
 
 
 
uint32_t THROTTLE;
uint32_t CONT_TEMP;
uint32_t MOT_TEMP;
 
uint32_t DC_CURRENT;
uint32_t AC_CURRENT;
uint32_t AC_VOLTAGE;
uint32_t DC_VOLTAGE;
 
uint32_t voltage1_hx;
uint32_t current1_hx;
 
uint32_t voltage2_hx;
uint32_t current2_hx;
 
uint32_t voltage3_hx;
uint32_t current3_hx;
 
uint32_t SOC1_hx;
uint32_t SOH1_hx;
 
uint32_t SOC2_hx;
uint32_t SOH2_hx;
 
uint32_t SOC3_hx;
uint32_t SOH3_hx;
 
uint32_t SOCAh_hx;
 
 
uint32_t Motor_err;
 
char bat1[32];
char bat2[32];
char bat3[32];
char vol_avg[32];
char sOC_avg[32];
char sOCAh[32];
 
char cur_tot[32];
char CVoltage_1[32];
char CVoltage_3[32];
char CVoltage_2[32];
char CCurrent_1[32];
char CCurrent_2[32];
char CCurrent_3[32];
char CSOC_1[32];
char CSOC_2[32];
char CSOC_3[32];
char CSOH_1[32];
char CSOH_2[32];
char CSOH_3[32];
char T_stamp[32];
char sensor[32];
char rpm[64];
char speed[64];
char throttle[64];
char motorTemp[64];
char contTmep[64];
char acCurrent[64];
char acVoltage[64];
char dcCurrent[64];
char dcVoltage[64];
char s_dcCurrent[64];
char s_acCurrent1[64];
char s_acCurrent2[64];
char trip1[64];
 
 
 
 
char Arduino[100];
 
 
 
 
 
 
float t1 = 0;
float t2 = 0;
 
/// ---------------------------------------------------------------------------
 
#define ECHO_TEST_TXD (GPIO_NUM_17)
#define ECHO_TEST_RXD (GPIO_NUM_16)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)
 
#define ECHO_UART_PORT_NUM (2)
#define ECHO_UART_BAUD_RATE (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE (CONFIG_EXAMPLE_TASK_STACK_SIZE)
 
static const char *TAG = "UART TEST";
 
#define BUF_SIZE (1024)
 
char arealx[16] = "";
char arealy[8] = "";
char arealz[8] = "";
char yaw[8] = "";
char pitch[8] = "";
char roll[8] = "";
 
char uno_data[150];
 
/////////////////////////////////////////////////////////////////////////////////
 
typedef enum
{
  TX_SEND_PINGS,
  TX_SEND_BATTERY_VI_OSC,
  TX_SEND_STOP_CMD,
  TX_TASK_EXIT,
} tx_task_action_t;
 
typedef enum
{
  RX_RECEIVE_COMPONENTS,
  RX_RECEIVE_DATA,
  RX_RECEIVE_STOP_RESP,
  RX_TASK_EXIT,
} rx_task_action_t;
 
static const twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
 
static const twai_general_config_t g_config = {.mode = TWAI_MODE_NORMAL,
                                               .tx_io = GPIO_NUM_21,
                                               .rx_io = GPIO_NUM_22,
                                               .clkout_io = TWAI_IO_UNUSED,
                                               .bus_off_io = TWAI_IO_UNUSED,
                                               .tx_queue_len = 10,
                                               .rx_queue_len = 20,
                                                .alerts_enabled = TWAI_ALERT_ALL,
                                               .clkout_divider = 0};
 
static QueueHandle_t tx_task_queue;
static QueueHandle_t rx_task_queue;
static SemaphoreHandle_t stop_ping_sem;
static SemaphoreHandle_t ctrl_task_sem;
static SemaphoreHandle_t done_sem;
static SemaphoreHandle_t ctrl_task_Transmit;
static SemaphoreHandle_t ctrl_task_receive;
static SemaphoreHandle_t ctrl_task_send;
 
/* --------------------------- Tasks and Functions -------------------------- */
 
static void twai_transmit_task(void *arg)
 
{
 
  //   tx_task_action_t action;
 
  //      xQueueReceive(tx_task_queue, &action, portMAX_DELAY);
 
 
 
  xSemaphoreTake(ctrl_task_Transmit, portMAX_DELAY); // Wait for completion
 
 
 
// xSemaphoreGive(ctrl_task_receive); // Start control task
 
 
 
  ESP_LOGI(EXAMPLE_TAG, "Transmitting to battery");
 
 
 
  vTaskDelay(pdMS_TO_TICKS(250));
  for (int f = 0; f < 50; f++)
 
    {
 
 
 
      twai_message_t transmit_message = {.identifier = (0x18F20309), .data_length_code = 8, .extd = 1, .data = {0x04, 0x00, 0x0, 0x30,0x0, 0x0, 0x0, 0x0
      }};
   
      if (twai_transmit(&transmit_message, 10000) == ESP_OK)
 
      {
 
 
 
        //          ESP_LOGI(EXAMPLE_TAG, "Message queued for transmission\n");
 
                 vTaskDelay(pdMS_TO_TICKS(100));
 
      }
    // twai_message_t transmit = {.identifier = (0x18530902 ), .data_length_code = 8, .extd = 1, .data = {0, 0, 0, 0,0, 0, 0, 0
    //   }};
   
    //   if (twai_transmit(&transmit, 10000) == ESP_OK)
 
    //   {
 
 
 
    //     //          ESP_LOGI(EXAMPLE_TAG, "Message queued for transmission\n");
 
    //              vTaskDelay(pdMS_TO_TICKS(250));
 
    //   }
     
      else
 
      {
 
 
 
        ESP_LOGE(EXAMPLE_TAG, "Failed to queue message for transmission\n");
 
      }
 
 
 
      vTaskDelay(pdMS_TO_TICKS(50));
 
    }
 
       vTaskDelay(pdMS_TO_TICKS(50));
 
 
 
  while (1)
 
  {
 
 
 
    // vTaskDelay(pdMS_TO_TICKS(150));
 
 
 
    //    if (action == TX_SEND_BATTERY_VI_OSC) {
 
    // Repeatedly transmit pings
 
    //  ESP_LOGI(EXAMPLE_TAG, "Transmitting to battery");
 
 
//twai_receive_task(*arg);
    for (int f = 0; f < 2; f++)
 
    {
 
 
 
      twai_message_t transmit_message = {.identifier = (0x18F20309 ), .data_length_code = 8, .extd = 1, .data = {0x25, 0x00, 0x00, 0x30,0x08, 0x02, 0x00, 0x00
      }};
   
      if (twai_transmit(&transmit_message, 10000) == ESP_OK)
 
      {
 
 
 
        //          ESP_LOGI(EXAMPLE_TAG, "Message queued for transmission\n");
 
                 vTaskDelay(pdMS_TO_TICKS(250));
 
      }
    // twai_message_t transmit = {.identifier = (0x18530902 ), .data_length_code = 8, .extd = 1, .data = {0, 0, 0, 0,0, 0, 0, 0
    //   }};
   
    //   if (twai_transmit(&transmit, 10000) == ESP_OK)
 
    //   {
 
 
 
    //     //          ESP_LOGI(EXAMPLE_TAG, "Message queued for transmission\n");
 
    //              vTaskDelay(pdMS_TO_TICKS(250));
 
    //   }
     
      else
 
      {
 
 
 
        ESP_LOGE(EXAMPLE_TAG, "Failed to queue message for transmission\n");
 
      }
 
 
 
      vTaskDelay(pdMS_TO_TICKS(50));
 
    }
 
 
 
    // for (int f = 0; f < 2; f++)
 
    // {
 
    //   twai_message_t transmit_message = {.identifier = (iDs[f] + 0x16), .data_length_code = 8, .extd = 1, .data = {0x07, 0, 0, 0, 0, 0, 0, 0}};
 
 
 
    //   if (twai_transmit(&transmit_message, 10000) == ESP_OK)
 
    //   {
 
    //     // printf("Message queued for transmission SOC\n");
 
    //   }
 
    //   else
 
    //   {
 
    //     // printf("Failed to queue message for transmission\n");
 
    //   }
 
 
 
    //   //  vTaskDelay(pdMS_TO_TICKS(100));
 
    // }
 
 
 
    //  }
 
 
 
    vTaskDelay(pdMS_TO_TICKS(50));
 
 
 
    /// /////////////////////////////////////////////////////////////////////////////
 
    ///
 
    //   else if (action == TX_TASK_EXIT) {
 
    // break;
 
    // }
 
  }
 
 
 
  xSemaphoreGive(done_sem);
 
 
 
  vTaskDelete(NULL);
 
}
 
 
static void twai_receive_task(void *arg)
{
  //       rx_task_action_t action;
  //     xQueueReceive(rx_task_queue, &action, portMAX_DELAY);
 
  xSemaphoreTake(ctrl_task_receive, portMAX_DELAY); // Wait for completion
  xSemaphoreGive(ctrl_task_send);                   // Start control task
 
  //  while (1) {
 
  //  vTaskDelay(pdMS_TO_TICKS(100));
 
  // if (action == RX_RECEIVE_COMPONENTS) {
 
  ESP_LOGI(EXAMPLE_TAG, "Reciving");
  //  vTaskDelay(pdMS_TO_TICKS(250));
 
  uint32_t MSG = 0;
 
 
  // Listen for ping response from slave
  while (1)
  {
    twai_message_t message;
    twai_receive(&message, pdMS_TO_TICKS(250));
    //           ESP_LOGI(EXAMPLE_TAG, "Reciving");
 
 
 
    if (MSG == message.identifier)
    {
      twai_clear_receive_queue();
 
      //printf("clearning RDX \n");
      //ESP_ERROR_CHECK(twai_stop());      
     
     
     // vTaskDelay(pdMS_TO_TICKS(1000));
 
 
     // ESP_ERROR_CHECK(twai_start());  
 
      MSG = 0;    
 
 
    }
 
        MSG = message.identifier;
 
 
//    ESP_LOGI(EXAMPLE_TAG, "message %lx",message.identifier);
 
 
  //    printf("mtr decode =  %x , %x , %x, %x , %x, %x, %x, %x  \n", message.data[0],message.data[1],message.data[2],message.data[3],message.data[4],message.data[5],message.data[6],message.data[7]);
 
 
    if (message.identifier == ID_MOTOR_RPM)
    { ///////    motor RPM
      // ESP_LOGI(EXAMPLE_TAG, " Motor RPM Data ");
      if (!(message.rtr))
      {
 
        // mcu_data = message.data;
 
        RPM = (message.data[1]) | (message.data[0] << 8);
 
            Motor_err = (message.data[7]) | (message.data[6]<<8) | (message.data[5]<<16)|(message.data[4]<<24) |(message.data[3]<<32)|(message.data[2]<<40) | (message.data[1]<<48)|(message.data[0]<<56);
 
            sprintf(motor_err, "%x,%x,%x,%x,%x,%x,%x,%x", message.data[0],message.data[1],message.data[2],message.data[3],message.data[4],message.data[5],message.data[6],message.data[7]);
 
 
        union
        {
          uint32_t b;
          int f;
        } u; // crazy
        u.b = RPM;
 
        M_RPM = u.f;
 
         twai_message_t transmit_message = {.identifier = (0x14520902 ), .data_length_code = 8, .extd = 1, .data = {RPM, 0, 0, DC_VOLTAGE, DC_CURRENT}};
      if (twai_transmit(&transmit_message, 1000) == ESP_OK)
      {
 
        //          ESP_LOGI(EXAMPLE_TAG, "Message queued for transmission\n");
        //          vTaskDelay(pdMS_TO_TICKS(250));
      }
      else
      {
 
        ESP_LOGE(EXAMPLE_TAG, "Failed to queue message for transmission\n");
      }
 
      vTaskDelay(pdMS_TO_TICKS(50));
       
 
 
 
      }
    }
 
    else if (message.identifier == ID_MOTOR_TEMP)
    { /////        motor temp
      // ESP_LOGI(EXAMPLE_TAG, " Motor Temp Data ");
      if (!(message.rtr))
      {
        THROTTLE = (message.data[0]);
 
        CONT_TEMP = (message.data[1]);
        MOT_TEMP = (message.data[2]);
        union
        {
          uint32_t b;
          int f;
        } u; // crazy
        u.b = THROTTLE;
        M_THROTTLE = u.f;
        u.b = CONT_TEMP;
        M_CONT_TEMP = u.f - 40;
        u.b = MOT_TEMP;
        M_MOT_TEMP = u.f - 40;
        //  printf("Temp =  %d \n", M_CONT_TEMP);
      }
    }
 
    else if (message.identifier == ID_MOTOR_CURR_VOLT)
    {
 
      DC_CURRENT = (message.data[0]) | (message.data[1] << 8);
      AC_CURRENT = (message.data[4]) | (message.data[5] << 8);
 
      AC_VOLTAGE = (message.data[3]);
 
      DC_VOLTAGE = (message.data[2]);
 
      union
      {
        uint32_t b;
        int f;
      } u; // crazy
      u.b = DC_CURRENT;
 
      M_DC_CURRENT = u.f;
 
      u.b = AC_CURRENT;
 
      M_AC_CURRENT = u.f;
 
      u.b = AC_VOLTAGE;
 
      M_AC_VOLTAGE = u.f;
 
      u.b = DC_VOLTAGE;
 
      M_DC_VOLATGE = u.f;
 
    } /////        motor temp
 
 
else if (message.identifier == ID_LX_BATTERY_SOC)
    { ////          Battery SOC 3
      if (!(message.rtr))
      {
        SOC3_hx = (message.data[1] << 8) | message.data[0];
        SOH3_hx = message.data[6]  ;
 
        SOCAh_hx = (message.data[5] << 24 | message.data[4] << 16 | message.data[3] << 8 | message.data[2]) ;
 
        union
        {
          uint32_t b;
          int f;
        } u; // crazy
        u.b = SOC3_hx;
        SOC_3 = u.f;
        u.b = SOH3_hx;
        SOH_3 = u.f;
 
        u.b = SOCAh_hx;
        SOCAh = u.f;
 
    //              printf("Battery 3 --> SOC[ %d ]   SOH[ %d ] \n", SOC_3,SOH_3);
      }
    }
 
else if (message.identifier == ID_LX_BATTERY_VI)
    { ////          Battery SOC 3
      if (!(message.rtr))
      {
       current2_hx  =  (message.data[0] << 24) | (message.data[1] << 16) | (message.data[2] << 8) | message.data[3];
        voltage2_hx =  (message.data[6] << 8) | (message.data[7] ) ;
 
   
       union
        {
          uint32_t b;
          int f;
        } u; // crazy
        u.b = voltage2_hx;
        Voltage_2 = u.f;
        u.b = current2_hx;
        Current_2 = u.f;
    //      printf("Battery 2 --> Volatge[%d]   current[%f]   \n", ( Voltage_2),(Current_2*0.001));
 
      }
    }
 
 
 
   else if (message.identifier == ID_LX_BATTERY_PROT)
    { ///////    motor RPM
      // ESP_LOGI(EXAMPLE_TAG, " Motor RPM Data ");
      if (!(message.rtr))
      {
 
        // mcu_data = message.data;
 
 
         //   BATT_ERR = (message.data[7]) | (message.data[6]<<8) | (message.data[5]<<16)|(message.data[4]<<24) |(message.data[3]<<32)|(message.data[2]<<40) | (message.data[1]<<48)|(message.data[0]<<56);
 
            sprintf(batt_err, "%x,%x,%x,%x,%x,%x,%x,%x", message.data[0],message.data[1],message.data[2],message.data[3],message.data[4],message.data[5],message.data[6],message.data[7]);
 
      }
    }
 
 
   else if (message.identifier == ID_LX_BATTERY_T)
    { ///////    motor RPM
      // ESP_LOGI(EXAMPLE_TAG, " Motor RPM Data ");
      if (!(message.rtr))
      {
 
        // mcu_data = message.data;
 
 
         //   BATT_ERR = (message.data[7]) | (message.data[6]<<8) | (message.data[5]<<16)|(message.data[4]<<24) |(message.data[3]<<32)|(message.data[2]<<40) | (message.data[1]<<48)|(message.data[0]<<56);
 
            sprintf(batt_temp, "%x,%x,%x,%x,%x,%x,%x,%x", message.data[0],message.data[1],message.data[2],message.data[3],message.data[4],message.data[5],message.data[6],message.data[7]);
 
      }
    }
 
    //*
 
 
 
    //*
 
 
    else
    {
      //   ESP_LOGE(EXAMPLE_TAG, " ID not match - %lx ",message.identifier );
      //   vTaskDelay(pdMS_TO_TICKS(250));
    }
 
   
       //   vTaskDelay(pdMS_TO_TICKS(50));
 
 
 
 
  }
  //   }
 
  //   else if (action == RX_TASK_EXIT) {
  //         break;
  //     }
  // }
 
  xSemaphoreGive(done_sem);
 
  vTaskDelete(NULL);
}
 
// void twai_control_task( )
 
static void twai_control_task(void *arg)
 
{
  // cd
  // tx_task_action_t tx_action;
  // rx_task_action_t rx_action;
 
  //   vTaskDelay(pdMS_TO_TICKS(1000));
 
  // ESP_ERROR_CHECK(twai_start());
  // ESP_LOGI(EXAMPLE_TAG, "Driver started");
 
  xSemaphoreTake(ctrl_task_sem, portMAX_DELAY); // Wait for completion
 
  //uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
  //int len = 0;
  //int count = 0;
 
  while (1)
  {
 
    // temp_sense1 = ds18b20_getTempC(tempSensor1);
    // temp_sense2 = ds18b20_getTempC(tempSensor2);
    // temp_sense3 = ds18b20_getTempC(tempSensor3);
    // temp_sense4 = ds18b20_getTempC(tempSensor4);
 
 
    // printf("-------------------------sensor  %f\n",temp_sense1);
 
    ///////////// temp clibration ////////////////////
 
    // temp_sense1 = (0.00566759*(temp_sense1*temp_sense1))+(0.4481*temp_sense1)+15.5782;
    // temp_sense2 = (0.00566759*(temp_sense2*temp_sense2))+(0.4481*temp_sense2)+15.5782;
    // temp_sense3 = (0.00566759*(temp_sense3*temp_sense3))+(0.4481*temp_sense3)+15.5782;
    // temp_sense4 = (0.00566759*(temp_sense4*temp_sense4))+(0.4481*temp_sense4)+15.5782;
 
    // memset(uno_data,0,strlen(uno_data));
 
    //    strcat(uno_data,"\",\"Arealx\":\"");
 
    /////////////////// uart recive
 
    //  ESP_ERROR_CHECK(uart_get_buffered_data_len(ECHO_UART_PORT_NUM, (size_t*)&len));
 
    // len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
 
    // if (len > 0)
    // {
    //   data[len] = '\0';
 
      // printf("%s\n", (char *)data);
 
      //        cJSON *root = cJSON_Parse((char *)data);
 
      // strcat(uno_data, cJSON_GetObjectItem(root,"Arealx")->valuestring);
 
      //                  sprintf(arealx, "%s ", (cJSON_GetObjectItem(root,"Arealx")->valuestring));
 
      //       printf( " arealx : %s\n", cJSON_GetObjectItem(root,"Arealx")->valuestring);
      //   strcat(uno_data, "\",\"Arealy\":\"");
 
      // strcat(uno_data, cJSON_GetObjectItem(root,"Arealy")->valuestring);
 
      //               sprintf(arealy, "%s ", (cJSON_GetObjectItem(root,"Arealy")->valuestring));
 
      //    strcat(uno_data, "\",\"Arealz\":\"");
 
      // strcat(uno_data, cJSON_GetObjectItem(root,"Arealz")->valuestring);
 
      //               sprintf(arealz, "%s ", (cJSON_GetObjectItem(root,"Arealz")->valuestring));
 
      // strcat(uno_data, "\",\"Pitch\":\"");
 
      //               sprintf(pitch, "%s ", (cJSON_GetObjectItem(root,"Pitch")->valuestring));
 
      // strcat(uno_data, cJSON_GetObjectItem(root,"Pitch")->valuestring);
 
      //                   strcat(uno_data, "\",\"Yaw\":\"");
 
      //                sprintf(yaw, "%s ", (cJSON_GetObjectItem(root,"Yaw")->valuestring));
 
      // strcat(uno_data, cJSON_GetObjectItem(root,"Yaw")->valuestring);
 
      //   strcat(uno_data, "\",\"Roll\":\"");
 
      //    strcat(uno_data, cJSON_GetObjectItem(root,"Roll")->valuestring);
 
      //            sprintf(roll, "%s ", (cJSON_GetObjectItem(root,"Roll")->valuestring));
 
      //    printf("----------------------------------------------end..... count = %s\n ",uno_data);
 
      //  printf("hhh ----------------------------------------\n ");
 
      //    uart_flush(ECHO_UART_PORT_NUM);
    //}
 
    // count = count + 1;
 
    // if (count > 300)
    // {
 
    //   uart_flush(ECHO_UART_PORT_NUM);
    //   count = 0;
    // }
 
    vTaskDelay(pdMS_TO_TICKS(100));
  }
 
  vTaskDelete(NULL);
 
  // uart_flush(ECHO_UART_PORT_NUM);
  // return;
}
 
static void senData(void *arg)
{
 
  // tx_task_action_t action;
  // xQueueReceive(tx_task_queue, &action, portMAX_DELAY);
 
  xSemaphoreTake(ctrl_task_send, portMAX_DELAY); // Wait for completion
  xSemaphoreGive(ctrl_task_sem);
 
  int count = 0;
 
  uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
 
  int len = 0;
 
  int axx = 0;
 
  while (1)
  {
 
 
    t2 = (esp_timer_get_time() / 1e+6);
 
    // if (action == TX_SEND_BATTERY_VI_OSC) {
 
    // battery ID decoding
    union
    {
      uint16_t b;
      int f;
    } u; // crazy
 
    u.b = iDs[0];
    sprintf(bat1, "%d ", ((u.f) / 32));
    u.b = iDs[1];
    sprintf(bat2, "%d ", ((u.f) / 32));
    u.b = iDs[2];
    sprintf(bat3, "%d ", ((u.f) / 32));
    //
 
    // averages and storts the data and sends it for logging
    // averaging and adding
 
    // voltage and SOC averaging
    float voltage_avg = Voltage_2*0.001;//((Voltage_1 + Voltage_2 + Voltage_3) / 3);
    int SOC_avg = SOC_3;
 
    sprintf(vol_avg, "%0.4f ", voltage_avg);
    sprintf(sOC_avg, "%d", SOC_avg);
 
 
    sprintf(sOCAh,"%0.4f",SOCAh);
    //
 
    // adding of currents from battery
    float current_tot = Current_2 * 0.001;//(-1) * (Current_1 + Current_3 + Current_2);
    sprintf(cur_tot, "%0.4f ", current_tot);
 
 
    //                      printf("mtr error =  %x , %x , %x, %x , %x, %x, %x, %x  \n", message.data[0],message.data[1],message.data[2],message.data[3],message.data[4],message.data[5],message.data[6],message.data[7]);
 
    // Individual conversions
 
   // sprintf(CVoltage_1, "%0.4f ", Voltage_1);
    //sprintf(CVoltage_2, "%0.4f ", Voltage_2);
  //  sprintf(CVoltage_3, "%0.4f ", Voltage_3);
 
   // sprintf(CCurrent_1, "%0.4f ", ((-1) * Current_1));
   // sprintf(CCurrent_2, "%0.4f ", ((-1) * Current_2));
   // sprintf(CCurrent_3, "%0.4f ", ((-1) * Current_3));
 
    //sprintf(CSOC_1, "%0.4f ", SOC_1);
    //sprintf(CSOC_2, "%0.4f ", SOC_2);
    //sprintf(CSOC_3, "%0.4f ", SOC_3);
 
    //sprintf(CSOH_1, "%0.4f ", SOH_1);
    //sprintf(CSOH_2, "%0.4f ", SOH_2);
    //sprintf(CSOH_3, "%0.4f ", SOH_3);
 
    sprintf(T_stamp, "%f ", (esp_timer_get_time() / 1e+6));
 
    ///  current sensor //////////////////////////////////////////////////
 
    float adc_i = ((((0.0000659375) * (-1)) * (adc_value * adc_value)) + (0.289911 * adc_value) - 310.881);
 
    // adc_i = ((0.7778739*adc_i)+1.53512);
 
    sprintf(sensor, "%0.4f ", adc_i);
 
    ///////////////////////////////////////////////////////////////////
 
    //////// RMP ////////////////
 
    sprintf(rpm, "%d ", M_RPM);
 
    ///////////////////////////
 
    /////////   speed ///////////////////
 
    sprintf(speed, "%0.4f ", (M_RPM * (0.016457)));
 
    /////////////////////////////////////
 
    ////// throttle, temps ///////////////
 
    sprintf(throttle, "%d ", M_THROTTLE);
 
    sprintf(motorTemp, "%d ", (M_MOT_TEMP)); // -40 to be added
 
    sprintf(contTmep, "%d ", (M_CONT_TEMP)); // -40 to be added
 
    ///////////////////////////////////
 
    ////////////  AC DC voltages and currents ///////////////
 
    sprintf(acCurrent, "%0.4f ", (M_AC_CURRENT * (0.1)));
 
    sprintf(acVoltage, "%d ", (M_AC_VOLTAGE));
 
    sprintf(dcCurrent, "%0.4f ", (M_DC_CURRENT * (0.1)));
 
    sprintf(dcVoltage, "%d ", (M_DC_VOLATGE));
 
    //////////////////////////////////////////////////////
 
    //////////////////// CUrrent Sensor ////////////////
 
    sprintf(s_dcCurrent, "%d ", (S_DC_CURRENT));
 
    sprintf(s_acCurrent1, "%d ", (S_AC_CURRENT1));
 
    sprintf(s_acCurrent2, "%d ", (S_AC_CURRENT2));
 
    /////////////////////////////////////////////
 
    /////////// trips /////////////////////////
 
    // TRIP1 = (((1435.608*((M_RPM/5.2)/60)*0.15)/1000)/1000);
 
    // TRIP1 =  (((((M_RPM/5.2)/60)*0.227)/0.15)/1000);
 
    TRIP1 = (((2 * 0.227 * 0.1885) * ((t2 - t1) / 3600) * M_RPM) * 2) / 10;
 
    TRIP_1 = TRIP_1 + TRIP1;
 
    sprintf(trip1, "%f ", (TRIP_1));
 
    ///////////////////////////////////////////
 
    sprintf(T_sense1, "%0.2f ", temp_sense1);
    sprintf(T_sense2, "%0.2f ", temp_sense2);
    sprintf(T_sense3, "%0.2f ", temp_sense3);
    sprintf(T_sense4, "%0.2f ", temp_sense4);
 
    // twai_control_task();
 
    //----------------------------------------------------------------
 
 
 
 
  len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
 
         if (len > 0) {
            data[len] = '\0';
           // uart_flush(ECHO_UART_PORT_NUM);
 
         }
 
 
      //  sprintf(Arduino, "%s ", (char *)data);
 
 
 
 
 
 
 
 
 
 
 
 
    char ii[32];
 
    char ir[32];
    sprintf(ii, "%d ", t_stamp);
 
    sprintf(ir, "%d ", t_stamp);
 
    char post_data[800] = "{\"api_key\":\"evLectrixScoo\",\"sensor\":\"ESP\",\"Speed\":\"";
 
    strcat(post_data, speed);
 
    strcat(post_data, "\",\"RPM\":\"");
 
    strcat(post_data, rpm);
 
    strcat(post_data, "\",\"Battery\":\"");
 
    strcat(post_data, sOC_avg);
 
    strcat(post_data, "\",\"Time\":\"");
 
    strcat(post_data, T_stamp);
 
    strcat(post_data, "\",\"BatteryTotalCurrent\":\"");
 
    strcat(post_data, cur_tot);
 
    // strcat(post_data, "56");
 
    strcat(post_data, "\",\"BatteryAvgVoltage\":\"");
 
    strcat(post_data, vol_avg);
 
    strcat(post_data, "\",\"SOCAh\":\"");
 
    strcat(post_data,     sOCAh);
 
 
 
    //strcat(post_data, "\",\"Battery_2_current\":\"");
 
    //strcat(post_data, CCurrent_2);
 
    //strcat(post_data, "\",\"Battery_3_current\":\"");
 
    //strcat(post_data, CCurrent_3);
 
    //strcat(post_data, "\",\"Battery_1_SOC\":\"");
 
    //strcat(post_data, CSOC_1);
 
    //strcat(post_data, "\",\"Battery_2_SOC\":\"");
 
    //strcat(post_data, CSOC_2);
 
    //strcat(post_data, "\",\"Battery_3_SOC\":\"");
 
    // strcat(post_data, CSOC_3);
 
    // strcat(post_data, "\",\"Battery_1_VOLT\":\"");
 
    // strcat(post_data, CVoltage_1);
 
    // strcat(post_data, "\",\"Battery_2_VOLT\":\"");
 
    // strcat(post_data, CVoltage_2);
 
    // strcat(post_data, "\",\"Battery_3_VOLT\":\"");
 
    // strcat(post_data, CVoltage_3);
 
    strcat(post_data, "\",\"Throttle_percentage\":\"");
 
    strcat(post_data, throttle);
 
    strcat(post_data, "\",\"Controller_Temp\":\"");
 
    strcat(post_data, contTmep);
 
    strcat(post_data, "\",\"Motor_temp\":\"");
 
    strcat(post_data, motorTemp);
 
    strcat(post_data, "\",\"Motor_DC_Current\":\"");
 
    strcat(post_data, dcCurrent);
 
    strcat(post_data, "\",\"Motor_AC_Current\":\"");
 
    strcat(post_data, acCurrent);
 
    strcat(post_data, "\",\"Motor_DC_Voltage\":\"");
 
    strcat(post_data, dcVoltage);
 
    strcat(post_data, "\",\"Motor_AC_Voltage\":\"");
 
    strcat(post_data, acVoltage);
 
    strcat(post_data, "\",\"Sensor_DC_Current\":\"");   //////
 
    strcat(post_data, s_dcCurrent);
 
    strcat(post_data, "\",\"Sensor_AC_Current_1\":\"");   /////
 
    strcat(post_data, s_acCurrent1);
 
    strcat(post_data, "\",\"Sensor_AC_Current_2\":\"");   ////
 
    strcat(post_data, s_acCurrent2);
 
 
 
 
    strcat(post_data, "\",\"TRIP_1\":\"");              
 
    strcat(post_data, trip1);
 
    strcat(post_data, "\",\"T_sense1\":\"");               ////
 
    strcat(post_data, T_sense1);
 
    strcat(post_data, "\",\"T_sense2\":\"");               ////
 
    strcat(post_data, T_sense2);
 
    strcat(post_data, "\",\"T_sense3\":\"");             ////
 
    strcat(post_data, T_sense3);
 
    strcat(post_data, "\",\"T_sense4\":\"");        ///
 
    strcat(post_data, T_sense4);
 
    strcat(post_data, "\",\"MotorERROR\":\"");  ////
 
    strcat(post_data, motor_err);
 
    strcat(post_data, "\",\"BatteryERROR\":\"");  ////
 
    strcat(post_data, batt_err);
 
   
    strcat(post_data, "\",\"Batterytemp\":\"");  ////
 
    strcat(post_data, batt_temp);
 
 
 
 
 
 
 
        //     strcat(post_data, Arduino);
 
    strcat(post_data, "\"}");
     printf("\n %s \n\n", post_data);
 
 
           //     printf("%s\n", (char *)data);
 
             //  printf("%s", (char *)data);
               
 
 
    //////////////////////////////////////////////// june 6 changes /////////////////////////////////////////////////
 
    //     cJSON *root = cJSON_Parse((char *)data);
 
    //          sprintf(arealx, "%s ", (cJSON_GetObjectItem(root,"Arealx")->valuestring));
 
    //           strcat(post_data, arealx);
 
    // cJSON_Delete(root);
 
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
 
    //    printf("\n %s \n\n", data);
 
   // printf("%s %s \n\n", post_data,(char *)data);
 
    // printf("count %d \n", count);
 
    //    vTaskDelay(10 / portTICK_PERIOD_MS);
 
    // sprintf(T_stamp, "%f ", (t2-t1));
 
    t1 = t2;
 
    count = count + 1;
 
    vTaskDelay(pdMS_TO_TICKS(200));
 
    //    if ( count > 110  )
    //  {
 
    //                  uart_flush(ECHO_UART_PORT_NUM);
 
    //      count = 2;
 
    // }
 
    //   else if (action == TX_TASK_EXIT) {
    //         break;
    //     }
  }
 
  vTaskDelete(NULL);
}
 
// static void twai_control_task(void *arg)
 
void app_main(void)
{
 
  uart_config_t uart_config = {
      .baud_rate = 115200,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  int intr_alloc_flags = 0;
 
#if CONFIG_UART_ISR_IN_IRAM
  intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif
 
  ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
 
  //  ESP_ERROR_CHECK(uart_set_word_length(ECHO_UART_PORT_NUM,UART_DATA_BITS_MAX));
 
  ESP_ERROR_CHECK(uart_set_mode(ECHO_UART_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));
 
  ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
 
  // Configure a temporary buffer for the incoming data
 
  /////////////////////////////////------------------------------------------------------------------------------------------------------------------------------
 
  //////////////////// temp
  // ds18b20_init(TEMP_BUS);
  // ds18b20_setResolution(tempSensor1, 1, 12);
  // ds18b20_setResolution(tempSensor2, 1, 12);
  // ds18b20_setResolution(tempSensor3, 1, 12);
  // ds18b20_setResolution(tempSensor4, 1, 12);
 
  // float cTemp = ds18b20_get_temp();
 
  // temp_sense1 = ds18b20_getTempC(tempSensor1);
  // temp_sense2 = ds18b20_getTempC(tempSensor2);
  // temp_sense3 = ds18b20_getTempC(tempSensor3);
  // temp_sense4 = ds18b20_getTempC(tempSensor4);
 
  ///////////////////////////////////////////
 
  // Create tasks, queues, and semaphores
  rx_task_queue = xQueueCreate(1, sizeof(rx_task_action_t));
  tx_task_queue = xQueueCreate(1, sizeof(tx_task_action_t));
  ctrl_task_Transmit = xSemaphoreCreateBinary();
  ctrl_task_receive = xSemaphoreCreateBinary();
  ctrl_task_send = xSemaphoreCreateBinary();
  ctrl_task_sem = xSemaphoreCreateBinary();
 
  stop_ping_sem = xSemaphoreCreateBinary();
  done_sem = xSemaphoreCreateBinary();
 
xTaskCreatePinnedToCore(twai_transmit_task, "TWAI_tx", 4096, NULL, TX_TASK_PRIO, NULL, tskNO_AFFINITY);
 
  xTaskCreatePinnedToCore(twai_receive_task, "TWAI_rx", 4096, NULL, RX_TASK_PRIO, NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(senData, "TWAI_rx", 4096, NULL, CTRL_TSK_PRIO, NULL, tskNO_AFFINITY);
 
// xTaskCreate(twai_control_task, "uart_echo_task", 4096, NULL, 8, NULL);
 
  //xTaskCreatePinnedToCore(twai_control_task, "UART_cnt", 4096, NULL, CTRL_TSK_PRIO, NULL, tskNO_AFFINITY);
 
  // Install TWAI driver
  ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));                                                            /// driver install
 
  ESP_LOGI(EXAMPLE_TAG, "Driver installed");
 
   ESP_ERROR_CHECK(twai_start());                                                                                                  /// driver start
 
 
  ESP_LOGI(EXAMPLE_TAG, "Driver started");
 
  vTaskDelay(pdMS_TO_TICKS(1000));
 
  xSemaphoreGive(ctrl_task_Transmit); // Start control task
                                      // Start control task
 
              xSemaphoreGive(ctrl_task_receive);        //////////////////////////////////////////////////////////////////////////////////////////      //Start control task
 
  // xSemaphoreGive(ctrl_task_sem);              //Start uart task
 
  //  twai_control_task();
 
  //   xSemaphoreGive(ctrl_task_sem);              //Start control task
  xSemaphoreTake(done_sem, portMAX_DELAY); // Wait for completion
 
  // Uninstall TWAI driver
  ESP_ERROR_CHECK(twai_driver_uninstall());
  ESP_LOGI(EXAMPLE_TAG, "Driver uninstalled");
 
  // Cleanup
  vQueueDelete(rx_task_queue);
  vQueueDelete(tx_task_queue);
  vSemaphoreDelete(ctrl_task_sem);
  vSemaphoreDelete(stop_ping_sem);
  vSemaphoreDelete(done_sem);
}