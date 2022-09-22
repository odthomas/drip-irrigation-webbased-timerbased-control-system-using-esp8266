#include <espressif/esp_common.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <espressif/spi_flash.h>
#include <sysparam.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include <httpd/httpd.h>
#include <ssid_config.h>

/* This below code is for DS3231  */
#include "i2c/i2c.h"
#include "ds3231/ds3231.h"
#define ADDR DS3231_ADDR
#define I2C_BUS 0
struct tm timeRTC;
float tempFloat;

/* END This below code is for DS3231  */




/* Add extras/sntp component to makefile for this include to work */
#include <sntp.h>
#include <sys/time.h>
#include <time.h>
#define SNTP_SERVERS 	"0.pool.ntp.org", "1.pool.ntp.org", "2.pool.ntp.org", "3.pool.ntp.org"
#define vTaskDelayMs(ms)	vTaskDelay((ms)/portTICK_PERIOD_MS)
char *servers[] = {SNTP_SERVERS};
const struct timezone tz = {5.5*60, 0};
/* pin config */
#define LED_PIN 2
#define VALVE_GPIO_PIN 14
/* gpio 0 usually has "PROGRAM" button attached */
const int progButton = 0;
int valveState;
time_t vtm;
time_t tt;
struct tm *voptm;
struct timeval *tv;
char response[400]; /* This array increased to 300 from 200 */
int len;
/* variables for sysparam functions*/
uint8_t *bin_value;
size_t param_len;
int32_t data;
uint32_t base_addr, num_sectors;
sysparam_status_t status;
uint32_t base_addr, num_sectors;
TimerHandle_t valveControlTimer;/*Variable for xTimerCreate*/




void switchOnValve()
{
    gpio_enable(VALVE_GPIO_PIN, GPIO_OUTPUT);
    gpio_write(VALVE_GPIO_PIN, true);
    valveState=1;
    vtm=time(NULL);
    voptm=localtime(&vtm);
}

void switchOffValve()
{
    gpio_enable(VALVE_GPIO_PIN, GPIO_OUTPUT);
    gpio_write(VALVE_GPIO_PIN, false);
    valveState=0;
    vtm=time(NULL);
    voptm=localtime(&vtm);
}

void switchOffLed(void)
{
    gpio_enable(LED_PIN, GPIO_OUTPUT);
    gpio_write(LED_PIN, true);
}

void switchOnLed(void)
{
    gpio_enable(LED_PIN,GPIO_OUTPUT);
    gpio_write(LED_PIN,false);
}



void sntpTask(void *pvParameters)
{
  const struct timezone tz = {5.5*60, 0};
  while (sdk_wifi_station_get_connect_status() != STATION_GOT_IP) {
    vTaskDelayMs(100);
  }
  printf("Starting SNTP... ");/* Start SNTP */
  sntp_set_update_delay(30*60000);/* SNTP will request an update each 5 minutes */
  /* Set GMT+1 zone, daylight savings off */
  sntp_initialize(&tz);/* SNTP initialization */
  sntp_set_servers(servers, sizeof(servers) / sizeof(char*));/* Servers must be configured right after initialization */
  printf("DONE!\n");
  vTaskDelete(NULL);
}




void websocket_task(void *pvParameters)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *) pvParameters;
   
    for (;;) {
        if (pcb == NULL || pcb->state != ESTABLISHED) {
            printf("Connection closed, deleting task\n");
            break;
        }
            
            /* Generate response in JSON format */
        
            /* /\* printf(response); *\/ */
        if (len < sizeof (response))
            websocket_write(pcb, (unsigned char *) response, len, WS_TEXT_MODE);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        
    }    
    vTaskDelete(NULL);
}

/**
 * This function is called when websocket frame is received.
 *
 * Note: this function is executed on TCP thread and should return as soon
 * as possible.
 */

void websocket_cb(struct tcp_pcb *pcb, uint8_t *data, u16_t data_len, uint8_t mode)
   {
    uint8_t response[2];
    uint16_t val;

    switch (data[0]) {
        case 'A': // ADC
            val = sdk_system_adc_read();/* This should be done on a separate thread in 'real' applications */
            break;
        case 'D': // Disable LED
            switchOffLed();
            switchOffValve();
            val = 0xDEAD;
            break;
        case 'E': // Enable LED
            switchOnLed();
            switchOnValve();
            val = 0xBEEF;
            break;
        default:
            printf("Unknown command\n");
            val = 0;
            break;
    }

    response[1] = (uint8_t) val;
    response[0] = val >> 8;
    websocket_write(pcb, response, 2, WS_BIN_MODE);
}

/**
 * This function is called when new websocket is open and
 * creates a new websocket_task if requested URI equals '/stream'.
 */
void websocket_open_cb(struct tcp_pcb *pcb, const char *uri)
{
    if (!strcmp(uri, "/stream")) {
        xTaskCreate(&websocket_task, "websocket_task", 256, (void *) pcb, 2, NULL);
    }
}


  /* This below code is for Linklist  04-05-2019 */

void print_forward_order();
int getListLength();
/* int getPeriod(); */
void print_list();
   



struct linked_list{
    int setTime;
    int number;
    struct linked_list *next;
    struct linked_list *previous;
};

typedef struct linked_list node;
node *head = NULL, *tail = NULL;



// Insert a node at tail of a circular doubly linked list
void insert_at_tail(int setTime,int number )
{
    node *newNode = (node *) malloc(sizeof(node));

    newNode->setTime=setTime;
    newNode->number = number;
  
    newNode->next = newNode;
    newNode->previous = newNode;

    if(head==NULL)
    {
        head = newNode;
        tail = newNode;
    }
    else
    {
        tail->next = newNode;
        newNode->next = head;
        newNode->previous = tail;
        tail = newNode;
        head->previous = tail;
    }
}

void vTimerCallback(TimerHandle_t xTimer)
{
    if(valveState==0){
         printf("Opening valve\n");
          xTimerChangePeriod(xTimer,head->setTime,0);
          printf("Open :: %d Period is running  ", head->setTime );
          switchOnValve();
         switchOnLed();

        }
    else{
          printf("Closing valve\n");
          printf("Close :: %d Period is running ", head->number);
          xTimerChangePeriod(xTimer, head->number,0);
          switchOffValve();
          switchOffLed();
          head=head->next;        
          }
}


//Determine the number of nodes in circular doubly linked list
int getListLength()
  {
    if(head==NULL) return 0;

    int count = 0;
    node *current = head;

    do
    {
        count++;
        current = current->next;
    }   while(current != head);

        return count;
   }



void print_forward_order()
{
    if(head==NULL)  return;
      node *current = head;

    do
    {
        printf("\n opened- %d ,closed-%d", current->setTime, current ->number);
        current = current->next;
    }   while(current != head);
    
     
        printf("\n openPeriod-%d ,ClosedPerod-%d", head->setTime,head->number);
        printf("\n List Length:(%d)\n", getListLength());
}


void display (int setTime, int number)
   {
     int count = 1;
     node *p;
     p = head;
    do
    {
     printf ("\n OpenTime= %d, closeTime =%d", p->setTime,p->number);
     count++;
     p = p->next;
     }while(p != head);
    printf ("\n");
  }


// Print the list in FORWARD order and REVERSE order

void print_list(int setTime, int number)
{
    printf("\n LINKLIST FORWARD ORDER PRINT:\n");
    print_forward_order( setTime, number);
    display( setTime,  number);
}


void rtcTask(void *pvParameters)
{
    /* struct tm timeRTC; */
    /* float tempFloat; */
    i2c_dev_t dev = {
        .addr = ADDR,
        .bus = I2C_BUS,
    };
    /* time.tm_mday=7; */
    /* time.tm_mon=10; */
    /* time.tm_year=118;  */
    /* ds3231_setTime(&dev, &time); */
    while(1) {
        vTaskDelay(100);
        ds3231_getTime(&dev, &timeRTC);
        ds3231_getTempFloat(&dev, &tempFloat);
        printf("\nRTC DATA \nDATE:%d/%d/%d,\nTIME:%d:%d:%d,\nTEMPERATURE:%.2f DegC\r\n",timeRTC.tm_mday+1, timeRTC.tm_mon+1, timeRTC.tm_year+1900, timeRTC.tm_hour, timeRTC.tm_min, timeRTC.tm_sec, tempFloat);

     
       

               node *current = head;
               current=current->next;
               print_list(current->setTime,current->number);
             
         
       
    }
}


void responseTask(void *pvParameters)
{

    int uptime, uptime_hour, uptime_min, uptime_sec,  heap, period ;
    
    struct tm *ptm;
    div_t r;
    while(1)
    {
        tt = time(NULL);
	data = tt;
        sysparam_set_int32("sys_time", data);
        ptm = localtime(&tt);
        uptime = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
        heap = (int) xPortGetFreeHeapSize();
        r=div(uptime,3600);
        uptime_hour=r.quot;
        uptime=r.rem;
        r=div(uptime, 60);
        uptime_min=r.quot;
        uptime_sec=r.rem;
        period = (int) head->number;
        /* Generate response in JSON format */
        len = snprintf(response, sizeof (response), "{\"presentTime\" : \"%2d hr:%2d min :%2d sec\"," 
                                                    "\"uptime\" : \"%2d hr:%2d min :%2d sec\","
                                                    " \"heap\" : \"%d\","
                                                    " \"led\" : \"%d\","
                                                    " \"valveState\" : \"%d\","
                                                    " \"prTime\" : \"%2d hr:%2d min:%2d sec\","
                                                    " \"datertc\" :\"%d - %d - %d\","
                                                    " \"temprtc\" : \"%.2f\" ,"
                                                    " \"timertc\" :\"%d : %d : %d\","
                                                    " \"period\"  :\"%d \" }", 
                                                    
                                                 
                                                 ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
                                                 uptime_hour, uptime_min, uptime_sec,
                                                 heap,
                                                 valveState,
                                                 valveState,
                                                 voptm->tm_hour, voptm->tm_min, voptm->tm_sec,
                                                 timeRTC.tm_mday+1, timeRTC.tm_mon+1, timeRTC.tm_year+1900,
                                                 tempFloat,
                                                 timeRTC.tm_hour, timeRTC.tm_min, timeRTC.tm_sec,
                                                 period
                                                 );
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}
  



void user_init(void)
{
    

    const int scl = 5, sda = 4;/* rtc code */
   
    i2c_init(0,scl,sda,I2C_FREQ_400K);/* rtc code */
    
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    status=sysparam_get_info(&base_addr, &num_sectors);
    printf("System parameters status = %d, base_address = %d, num_sectors = %d\n", status, base_addr, num_sectors);
    status=sysparam_get_int32("sys_time",&data);
    vtm=data;
    voptm=localtime(&vtm);
    printf("%2d:%2d:%2d",voptm->tm_hour, voptm->tm_min, voptm->tm_sec);
    valveState=0;
    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };
    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);
    websocket_register_callbacks((tWsOpenHandler) websocket_open_cb, (tWsHandler) websocket_cb);
    httpd_init();

  

 



    int a= 100*60*1.0, b=100*60*90;  /*1min on time, 90min stop time*/
    int c= 100*60*1.0, d=100*60*90;  /*1min on time, 90min stop time*/
    int e= 100*60*1.0, f=100*60*90;  /*1min on time, 90min  stop time*/  
    int g= 100*60*1.0, h=100*60*1167;  /*1min on time, 1hours stop time*/
    

    insert_at_tail(a,b);
    insert_at_tail(c,d);
    insert_at_tail(e,f);
    insert_at_tail(g,h);
    

     

    /*Create a timer with handle in valveControlTimer and callback function in vTimerCallback*/
    valveControlTimer=xTimerCreate("ControlTimer",pdMS_TO_TICKS(a), pdTRUE, (void *)0, vTimerCallback );
    xTimerStart(valveControlTimer,0);
    /* initialize tasks */
    xTaskCreate(responseTask, "responseTask", 768, NULL, 2, NULL);
    xTaskCreate(sntpTask, "sntpTask", 768, NULL, 2,NULL); 
    xTaskCreate(rtcTask, "rtcTask", 256, NULL, 2, NULL);
}

