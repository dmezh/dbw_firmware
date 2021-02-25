#include <iostream>
#include <freertos/FreeRTOS.h>
#include "esp_spi_flash.h"
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <string.h>
#include "Cantask.h"
#include <ESP32CAN.h>
#include <CAN_config.h>
#include <freertos/queue.h>
#include "queues.h"
#include "MessageID.h"

//Minimum free stack space using watermark function uxTaskGetStackHighWaterMark() was 488 bytes
//Occupied stack size is about 1500 bytes

CAN_device_t CAN_cfg;
CAN_frame_t tx_frame, rx_frame;
char i = 33;

void sendToTask(QueueHandle_t TaskQueue){
  //portMAX_DELAY means you wait indefinitely until the queue is able to receive messages; the third parameter is how long you wait is th queue is full
  xQueueSend(TaskQueue, &rx_frame, portMAX_DELAY);
}

void sendMessage() {
  tx_frame.data.u8[7] = i;
  //printf("sent %d, %.8s\n", i, tx_frame.data.u8);
  ESP32Can.CANWriteFrame(&tx_frame);
}

void checkMessage() {
  if(xQueueReceive(CAN_cfg.rx_queue,&rx_frame, 0)==pdTRUE){
    //temporary measure; normally we analyze rx_frame and send it to the task that needs it
    std::string Message;
    for(int i = 0; i < 8; i++){
      //puts the message into a string data-type
      Message.push_back((char)rx_frame.data.u8[i]);
    }
    /*if(Message.compare("EMERSTOP") == 0){
      es_frame = rx_frame;
      sendToEstop();
    }*/
    if (rx_frame.MsgID > taskID::ESTOP_RANGE_START && rx_frame.MsgID < taskID::ESTOP_RANGE_END){
      sendToTask(canToEStop);
    }
    else if(rx_frame.MsgID > taskID::HOUSE_RANGE_START && rx_frame.MsgID < taskID::HOUSE_RANGE_END){
      sendToTask(canToHouse);
    }
    else if(rx_frame.MsgID >  taskID::WATCH_RANGE_START && rx_frame.MsgID < taskID::WATCH_RANGE_END){
      sendToTask(canToWatch);
    }
    printf("New %s frame", (rx_frame.FIR.B.FF==CAN_frame_std ? "standard" : "extended"));
    if(rx_frame.FIR.B.RTR==CAN_RTR) printf(" RTR");
    printf(" from 0x%08x, DLC %d\r\n",rx_frame.MsgID,  rx_frame.FIR.B.DLC);
    printf("Cantask received: ");
    for(int i = 0; i < 8; i++) printf("%c", (char)rx_frame.data.u8[i]);
    printf("\n");
  }
}

void cantask( void *pvParamters){
    tx_frame.FIR.B.FF = CAN_frame_std;
    tx_frame.MsgID = 0x101;
    tx_frame.FIR.B.DLC = 8;
    strcpy((char*)tx_frame.data.u8, "mymsg: ");
    CAN_cfg.speed=CAN_SPEED_1000KBPS;
    CAN_cfg.tx_pin_id = GPIO_NUM_25;
    CAN_cfg.rx_pin_id = GPIO_NUM_26;
    //cantask receives commands and places them into this queue
    CAN_cfg.rx_queue = xQueueCreate(10,sizeof(CAN_frame_t));
    ESP32Can.CANInit();

    for(;;){
        if (i >= 127) i = 33;
        i++;
        sendMessage();
        checkMessage();
        esp_task_wdt_reset();
        vTaskDelay(250/portTICK_PERIOD_MS);
    }
}