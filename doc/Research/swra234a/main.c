/******************************************************************************
    Filename: main.c
******************************************************************************/

#include <hal_types.h>
#include <hal_defs.h>
#include <hal_board.h>
#include <hal_lcd.h>
#include <hal_uart.h>
#include <hal_led.h>
#include <hal_spi.h>
#include <hal_int.h>
#include <hal_mcu.h>
#include <hal_rf.h>
#include <cc2500.h>
#include <stdlib.h>
#include <mbus_packet.h>
#include <tx.h>
#include <rx.h>

#include <tmode_rf_settings.h>
#include <smode_rf_settings.h>

//----------------------------------------------------------------------------------
//  Constants used in this file
//----------------------------------------------------------------------------------

// Radio Mode
#define RADIO_MODE_TX  1
#define RADIO_MODE_RX  2



//----------------------------------------------------------------------------------
//  Variables
//----------------------------------------------------------------------------------

static volatile uint8 radioMode;
static volatile uint8 radioModeSet = FALSE;

static volatile uint8 WMBUSmode;
static volatile uint8 WMBUSmodeSet = FALSE;

static volatile uint8 buttonPushed = FALSE;


// Pin to indicate active or low Power Mode
// Can be removed in a final implementation
const digioConfig pinLPM = {3, 4, BIT4, HAL_DIGIO_OUTPUT, 1};


// TX - Buffers
uint8 TXdata[256];
uint8 TXdataLength = 5;
uint8 TXpacket[291];
uint8 TXbytes[584];

// RX - Buffers
uint8 RXpacket[291];
uint8 RXbytes[584];



//----------------------------------------------------------------------------------
//  Functions
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
//  void myS1buttonISR(void)
//
//  DESCRIPTION:
//    This function is called when the S1 button is pressed.
//    Selects mode of operation the first time it runs.
//    Selects WMBUS mode the second time it runs.
//----------------------------------------------------------------------------------

static void myS1buttonISR(void)
{
  if (!radioModeSet)
  {
      radioMode = RADIO_MODE_RX;
      radioModeSet = TRUE;
      halLcdWriteSymbol(HAL_LCD_SYMBOL_RX, TRUE);
  }
  else if (!WMBUSmodeSet)
  {
      WMBUSmode = WMBUS_SMODE;
      WMBUSmodeSet = TRUE;
      halLcdWriteSymbol(HAL_LCD_SYMBOL_ANT, TRUE);
  }
  buttonPushed = TRUE;
  
 // Simple debouncing logic
 // A final application should use a more advanced version
 halMcuWaitUs(60000); 
 halDigioIntClear(&pinS1);
 
}



//----------------------------------------------------------------------------------
//  void myS2buttonISR(void)
//
//  DESCRIPTION:
//    This function is called when the S2 button is pressed.
//    Selects mode of operation the first time it runs.
//    Selects WMBUS mode the second time it runs.
//----------------------------------------------------------------------------------

static void myS2buttonISR(void)
{
  if (!radioModeSet)
  {
      radioMode = RADIO_MODE_TX;
      radioModeSet = TRUE;
      halLcdWriteSymbol(HAL_LCD_SYMBOL_TX, TRUE);
  }
  else if (!WMBUSmodeSet)
  {
      WMBUSmode = WMBUS_TMODE;
      WMBUSmodeSet = TRUE;
      halLcdWriteSymbol(HAL_LCD_SYMBOL_ANT, TRUE);
  }
  
  buttonPushed = TRUE;

  // Simple debouncing logic                               
  // A final application should use a more advanced version
  halMcuWaitUs(60000); 
  halDigioIntClear(&pinS2);

}



//----------------------------------------------------------------------------------
//  void initData (uint8* pData, uint8 size)
//
//  DESCRIPTION:
//    Set up example user data for a wireless MBUS packet in an array
//
//  ARGUMENTS:  
//    uint8 pData         - Pointer to the array
//    unit8 size          - Size of the array
//----------------------------------------------------------------------------------
static void initData (uint8* pData, uint8 size)
{
  
  // Create a Wireless MBUS packet - Test Only
  // Loop through all possible packet sizes. 
/*  uint8 dataCnt;
  // Fill array with dummy data
  for (dataCnt = 0; dataCnt < size; dataCnt++)
    pData[dataCnt] = dataCnt;
*/
  
  // Create a Wireless MBUS packet with valid data
  pData[0] = 0x0B;
  pData[1] = 0x12;
  pData[2] = 0x43;
  pData[3] = 0x65;
  pData[4] = 0x87;

}



//----------------------------------------------------------------------------------
//  uint8 asciiValue hexToAscii(uint8 hexValue)
//
//  DESCRIPTION:
//    Converts a 4-bit hex value to a ascii character
//
//  ARGUMENTS:  
//    uint8 hexValue      - pointer to the HEX value
//
//  RETURNS:  
//    The ASCII character
//----------------------------------------------------------------------------------
static uint8 hexToAscii(uint8 hexValue)
{
  uint8 asciiValue;
  
  asciiValue = (hexValue & 0x0F);
  
  // If the hex value is [0-9]
  if (asciiValue < 10 )
    asciiValue += 48; // ASCII '0' -> 48 decimal
  
  // Else the hex value is [A-F]
  else 
    asciiValue += 55; // ASCII 'A' -> 65 decimal
  
  return (asciiValue);
  
}




//----------------------------------------------------------------------------------
//  void main(void)
//
//  DESCRIPTION:
//    This is the main entry of the "Wireless MBUS" application. It sets up the board
//    and lets the user select operating mode (RX or TX) by pressing either button
//    S1 (RX) or S2(TX). After selecting operating mode, the user selects Wireless 
//    MBUS mode (Smode or Tmode) by pressing either button S1 (Smode) or S2 (Tmode)
//
//    In TX mode, a wireless MBUS packet is written to the FIFO 
//    every time S2 is pushed. The LCD will be updated every time a new 
//    packet is successfully transmitted. If an error occurs during transmit, 
//    a LED will blink.
//
//    In RX mode, the LCD will be updated every time a new 
//    packet is successfully received. The received packet will also be transmitted
//    over the UART.
//    If an error occurs during reception, a LED will blink.
//----------------------------------------------------------------------------------
void main (void)
{
  
  //------------------------------------------------------------------------------
  // declarations
  //------------------------------------------------------------------------------
  

  // Tranceiver ID
  uint8  id;
  uint8  ver;

  
  //------------------------------------------------------------------------------
  // Initialization
  //------------------------------------------------------------------------------
  
  // Init board and LPM pin
  halDigioConfig(&pinLPM);
  halBoardInit();

  // Reset the CC1101 Tranceiver
  halRfResetChip();

  // Display CC1101 ID and Version
  id  = halRfGetChipId();
  ver = halRfGetChipVer();
  halLcdWriteValue((id << 8) | ver, HAL_LCD_RADIX_HEX, 0);

  // Put radio to sleep
  halRfStrobe(CC2500_SPWD);

  // Set up interrupts for events on the S1 and S2 buttons
  halDigioIntSetEdge(&pinS1, HAL_DIGIO_INT_RISING_EDGE);
  halDigioIntConnect(&pinS1, &myS1buttonISR);
  halDigioIntEnable(&pinS1);

  halDigioIntSetEdge(&pinS2, HAL_DIGIO_INT_RISING_EDGE);
  halDigioIntConnect(&pinS2, &myS2buttonISR);
  halDigioIntEnable(&pinS2);
  
  
  //------------------------------------------------------------------------------
  // Mode Selection
  //------------------------------------------------------------------------------
  
  // Wait for user to select operating mode
  while (!buttonPushed)
  {
    halMcuSetLowPowerMode(HAL_MCU_LPM_3); // Will Enable Interrupts
  }

  buttonPushed = FALSE;
  
  // Wait for user to select WMBUS mode
  while (!buttonPushed)
  {
    halMcuSetLowPowerMode(HAL_MCU_LPM_3); // Will Enable Interrupts
  }

  buttonPushed = FALSE;


  //------------------------------------------------------------------------------
  // TX Mode
  // Data in the TXdata buffer is encoded into a Wireless MBUS packet. 
  // The packet will be encoded (according to mode) and transmitted each time the 
  // S2 button is pushed.    
  //------------------------------------------------------------------------------
      
  if (radioMode == RADIO_MODE_TX)
  {
    uint16 counter;
    uint16 status;

    counter = 0;

    // Create a Wireless MBUS packet
    initData(TXdata, TXdataLength);
    encodeTXPacket(TXpacket, TXdata, TXdataLength);

    // S-mode selected
    if (WMBUSmode == WMBUS_SMODE)
      txInitMode(WMBUS_SMODE, &sModeRfConfig, sModePaTable, sModePaTableLen);
    
    // T-mode selected
    else if (WMBUSmode == WMBUS_TMODE)
      txInitMode(WMBUS_TMODE, &tModeRfConfig, tModePaTable, tModePaTableLen);
      
    while (TRUE)
    {
      // Create a Wireless MBUS packet - Test Only
      // Loop through all possible packet sizes.
/*      initData(TXdata, TXdataLength);
      encodeTXPacket(TXpacket, TXdata, TXdataLength);
      TXdataLength++;
      if (TXdataLength > 0xF5)
       TXdataLength = 0;
*/              
      // Transmit packet
      status = txSendPacket(TXpacket, TXbytes, WMBUSmode);
      
      // Update display if packet successfully transmitted
      if (status == TX_OK)
      {
        // Display number of packets sent
        halLcdWriteValue(++counter, HAL_LCD_RADIX_HEX, 0);  
      }
      
      // Blink a LED if transmission failed
      else
      {
        halLedToggle(1);
        halMcuWaitUs(20000);
        halLedToggle(1);
      }
      
      halDigioIntEnable(&pinS1);
      halDigioIntEnable(&pinS2);
      
      // Await user to start a new transmission
      while (buttonPushed == FALSE)
      {
        // Clear LPM pin to indicate LPM mode  
        halDigioClear(&pinLPM);
        halMcuSetLowPowerMode(HAL_MCU_LPM_3); // Will Enable Interrupts
      }
      
      // The pin interrupts are disabled to ensure:
      // - Avoid triggering a new transmission before 
      // the current is completed. 
      // - Avoid transmission failure due to the debouncing logic. 
      halDigioIntDisable(&pinS1);
      halDigioIntDisable(&pinS2);
      buttonPushed = FALSE;

    }
  }
    
    
  //------------------------------------------------------------------------------
  // RX mode 
  // Initialize the receiver and wait for a packet to be received. 
  // When a complete packet received, the packet is decoded.
  // The LCD display will be updated if 
  // - No data decoding errors
  // - No CRC errors
  // - No RX errors 
  //------------------------------------------------------------------------------
  
  else if (radioMode == RADIO_MODE_RX)
  {
    uint16 counter;
    uint16 status;
    counter = 0;
    
    // - Initialize receiver -
    // Send a text sting over the UART
    halUartWrite( "WIRELESS MBUS PACKET RECEIVED\r\n", 31);
    
    // S-mode selected
    if (WMBUSmode == WMBUS_SMODE)
      rxInitMode(WMBUS_SMODE, &sModeRfConfig, sModePaTable, sModePaTableLen);

    // T-mode selected
    else if (WMBUSmode == WMBUS_TMODE)
      rxInitMode(WMBUS_TMODE, &tModeRfConfig, tModePaTable, tModePaTableLen);
    
    // - Indicate status
    while (TRUE)
    {
      // Await packet received
      status = rxRecvPacket(RXpacket, RXbytes, WMBUSmode); 

       // Update display if packet successfully received and decoded
      if (status == PACKET_OK)
      {
        // Display number of packets received
        halLcdWriteValue(++counter, HAL_LCD_RADIX_HEX, 0);
        
        // Send the received Wireless MBUS packet to the UART
        // The HEX values are converted to ASCII characters so the that 
        // HyperTerminal can be used.
        int i;
        uint8 asciiChar;

        for (i=0; i < packetSize(RXpacket[0]); i++)
        {
          asciiChar = hexToAscii((RXpacket[i] >> 4));
          halUartWrite( &asciiChar, 1);
          asciiChar = hexToAscii(RXpacket[i]);
          halUartWrite( &asciiChar, 1);

          // Space character
          halUartWrite( " ", 1);
        }          
        
        // 2x End of line 
        halUartWrite( "\r\r\n\n", 4);
      }

      // Blink led if receive failed
      else
      {
        halLedToggle(1);
        halMcuWaitUs(20000);
        halLedToggle(1);
      }
    }
  }   
}

  
/***********************************************************************************
  Copyright 2008 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
***********************************************************************************/


