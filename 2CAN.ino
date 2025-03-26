#include <mcp_can.h>
#include <SPI.h>
#include <avr/wdt.h>

byte EEMEM filterEnabled_addr;

unsigned long currentMillis;

byte filterEnabled = 1;
byte ecoStatusCurr = 0;
byte ecoStatusPrev = 2;
byte ecoChangeStep = 0;
unsigned long ecoTimer = 0;
bool acecuStatusCurr;
bool etacsStatusCurr;
unsigned long statusTimer;

unsigned long rxId;
byte len;
byte rxBuf[8];

MCP_CAN CAN0(10);                              // CAN0 interface usins CS on digital pin 10
MCP_CAN CAN1(9);                               // CAN1 interface using CS on digital pin 9

#define DATA_LIMIT 8

// структура хранения данных CAN-пакета
struct CANFrame
{
  uint32_t ID;                // идентификатор пакета
  uint8_t  Length;            // длина данных
  uint8_t  Data[DATA_LIMIT];  // сами данные
};

CANFrame OutFrame;

void setup()
{
	
  acecuStatusCurr = false;
  etacsStatusCurr = false;
  statusTimer = 0;
	
  Serial.begin(115200);
  
  // init CAN0 bus, baudrate: 250k@8MHz
  if(CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK){
    Serial.print("CAN0: Init OK!\r\n");
    CAN0.setMode(MCP_NORMAL);
  } else Serial.print("CAN0: Init Fail!!!\r\n");
  
  // init CAN1 bus, baudrate: 250k@8MHz
  if(CAN1.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK){
    Serial.print("CAN1: Init OK!\r\n");
    CAN1.setMode(MCP_NORMAL);
  } else Serial.print("CAN1: Init Fail!!!\r\n");
  
  //SPI.setClockDivider(SPI_CLOCK_DIV2);         // Set SPI to run at 8MHz (16MHz / 2 = 8 MHz)
  
  filterEnabled = 1;
  //filterEnabled = eeprom_read_byte(&filterEnabled_addr);
  //if((filterEnabled!=0)||(filterEnabled!=1))
  //{
    //Инициализация в епром
  //  filterEnabled = 1;
    //eeprom_write_byte(&filterEnabled_addr, filterEnabled);
  //};
	
  statusTimer = millis();
  wdt_enable(WDTO_2S);

}

void loop(){
  currentMillis = millis();
  wdt_reset();
  
  if(!digitalRead(2)){                         // If pin 2 is low, read CAN0 receive buffer
    //CAN0.readMsgBuf(&rxId, &len, rxBuf);       // Read data: len = data length, buf = data byte(s)
   
    if (CAN0.checkReceive() == CAN_MSGAVAIL) {
      CAN0.readMsgBuf(&OutFrame.ID, &OutFrame.Length, OutFrame.Data);
    }
    if(filterEnabled==1)
    {
      filter(&OutFrame);
    }
    CAN1.sendMsgBuf(OutFrame.ID, 0, OutFrame.Length,  OutFrame.Data);      // Immediately send message out CAN1 interface
    readStatus(&OutFrame);
  }
  if(!digitalRead(3)){                         // If pin 3 is low, read CAN1 receive buffer
    CAN1.readMsgBuf(&OutFrame.ID, &OutFrame.Length, OutFrame.Data);       // Read data: len = data length, buf = data byte(s)
    if (CAN1.checkReceive() == CAN_MSGAVAIL) {
      CAN1.readMsgBuf(&OutFrame.ID, &OutFrame.Length, OutFrame.Data);
    }
    if(filterEnabled==1)
    {
      filter(&OutFrame);
    }
    CAN0.sendMsgBuf(OutFrame.ID, 0, OutFrame.Length,  OutFrame.Data);      // Immediately send message out CAN0 interface
    readStatus(&OutFrame);
  }
}

void filter(CANFrame *msg)
{
  if(msg->ID == 0x185){
	//Подавление команды на запуск ДВС с климата  
    msg->Data[1] = 0x00;
  }
}

void readStatus(CANFrame *msg)
{
  if(msg->ID == 0x359)
  {
    if(bit_is_set(msg->Data[6], 0)) 
    {
      //ЭКО включен
      ecoStatusCurr = 1;
      //if(ecoStatusPrev!=ecoStatusCurr) tone(PIN_SPEAKER, 800, 50);
    }
    if(bit_is_set(msg->Data[6], 1)) 
    {
      //ЭКО отключен
      ecoStatusCurr = 0;
      //if(ecoStatusPrev!=ecoStatusCurr) tone(PIN_SPEAKER, 1000, 50);
    }
 
    if(ecoStatusPrev!=ecoStatusCurr)
    {
      //ЭКО включили или выключили
       
      if(ecoChangeStep==0)
      {
        ecoTimer = currentMillis;
      }
      if((currentMillis - ecoTimer) < 4000)
      {
        ecoChangeStep++;
      }
      else
      {
        ecoChangeStep = 0;
        //ecoTimer = 0;
      }
              
      if(ecoChangeStep>=3)
      {
        //Инвертируем состояние фильтра
        filterEnabled = !filterEnabled;
        //eeprom_write_byte(&filterEnabled_addr, filterEnabled);
        ecoChangeStep = 0;
        ecoTimer = 0;
      }
      
    }
    ecoStatusPrev = ecoStatusCurr;
  }
  if(msg->ID == 0x185) 
  {
    acecuStatusCurr = true;
  }
  if(msg->ID == 0x154)
  {
    etacsStatusCurr = true;
  }
}
