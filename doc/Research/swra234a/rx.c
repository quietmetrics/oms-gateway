/***********************************************************************************
    Filename: rx.c
***********************************************************************************/

#include <hal_types.h>
#include <hal_defs.h>
#include <hal_mcu.h>
#include <hal_int.h>
#include <hal_board.h>
#include <hal_rf.h>
#include <cc2500.h>
#include <manchester.h>
#include <3outof6.h>
#include <mbus_packet.h>
#include <rx.h>

//----------------------------------------------------------------------------------
//  Note:
//  The CC2500 RF register map is identical to the CC1101 RF register map.
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
//  Variables used
//----------------------------------------------------------------------------------
RXinfoDescr RXinfo;
extern const digioConfig pinLPM;


//----------------------------------------------------------------------------------
//  void rxFifoISR()
//
//  DESCRIPTION:
//    This function is called when the FIFO Threshold signal is asserted indicating
//    that the FIFO (in this example) is
//    - Half full 
//    - 4 bytes are received
//   
//    When 4 bytes received detected, the first 3 bytes are read out, 
//    and the length of the packet is calculated. The FIFO threshold signal is 
//    set to be half full. Length mode is set to FIXED when less than 
//    256 bytes left of the packet.
//
//    When half full the FIFO is read, and length mode is set to FIXED when less than 
//    256 bytes left of the packet.
//----------------------------------------------------------------------------------

void rxFifoISR()
{

  uint8 fixedLength;
  uint8 bytesDecoded[2];
  
  // Set LPM pin to indicate active mode
  halDigioSet(&pinLPM);
    
  // - RX FIFO 4 bytes detected - 
  // Calculate the total length of the packet, and set fixed mode if less
  // than 255 bytes to receive
  if (RXinfo.start == TRUE)
  {  
    // Read the 3 first bytes
    halRfReadFifo(RXinfo.pByteIndex, 3);
    
    // - Calculate the total number of bytes to receive -
    // S-Mode
    if (RXinfo.mode == WMBUS_SMODE)
    {
      // Possible improvment: Check the return value from the deocding function,
      // and abort RX if coding error. 
      manchDecode(RXinfo.pByteIndex, bytesDecoded);
      RXinfo.lengthField = bytesDecoded[0];
      RXinfo.length = byteSize(1, 0, (packetSize(RXinfo.lengthField)));
    }
    // T-Mode
    else  
    {
      // Possible improvment: Check the return value from the deocding function,
      // and abort RX if coding error. 
      decode3outof6(RXinfo.pByteIndex, bytesDecoded, 0);
      RXinfo.lengthField = bytesDecoded[0];
      RXinfo.length = byteSize(0, 0, (packetSize(RXinfo.lengthField)));
    }
    
    // - Length mode - 
    // Set fixed packet length mode is less than 256 bytes
    if (RXinfo.length < (MAX_FIXED_LENGTH))
    {
      halRfWriteReg(CC2500_PKTLEN, (uint8)(RXinfo.length));
      halRfWriteReg(CC2500_PKTCTRL0, FIXED_PACKET_LENGTH);
      RXinfo.format = FIXED;
    }
    
    // Infinite packet length mode is more than 255 bytes
    // Calculate the PKTLEN value
    else
    {
      fixedLength = RXinfo.length  % (MAX_FIXED_LENGTH);
      halRfWriteReg(CC2500_PKTLEN, (uint8)(fixedLength));    
    } 
                
    RXinfo.pByteIndex += 3;
    RXinfo.bytesLeft   = RXinfo.length - 3;
    
    // Set RX FIFO threshold to 32 bytes
    RXinfo.start = FALSE;
    halRfWriteReg(CC2500_FIFOTHR, RX_FIFO_THRESHOLD);
  }
  
  // - RX FIFO Half Full detected - 
  // Read out the RX FIFO and set fixed mode if less
  // than 255 bytes to receive
  else
  {
    // - Length mode -
    // Set fixed packet length mode is less than 256 bytes
    if (((RXinfo.bytesLeft) < (MAX_FIXED_LENGTH )) && (RXinfo.format == INFINITE))
    {
      halRfWriteReg(CC2500_PKTCTRL0, FIXED_PACKET_LENGTH);   
      RXinfo.format = FIXED;
    }

    // Read out the RX FIFO
    // Do not empty the FIFO (See the CC110x or 2500 Errata Note)
    halRfReadFifo(RXinfo.pByteIndex, RX_AVAILABLE_FIFO - 1);

    RXinfo.bytesLeft  -= (RX_AVAILABLE_FIFO - 1);
    RXinfo.pByteIndex += (RX_AVAILABLE_FIFO - 1);
  }
}



//----------------------------------------------------------------------------------
//  void rxPacketRecvdISR(void)
//
//  DESCRIPTION:
//    This function is called when the complete packet has been received. 
//    The remaining bytes in the RX FIFO are read out, and 
//    packet complete signalized
//----------------------------------------------------------------------------------

void rxPacketRecvdISR(void)
{
  
  // Set LPM pin to indicate active mode
  halDigioSet(&pinLPM);
  
  // Read remaining bytes in RX FIFO
  halRfReadFifo(RXinfo.pByteIndex, (uint8)RXinfo.bytesLeft); 
  RXinfo.complete = TRUE;
    
  // Clear potential RX FIFO Threshold interrupt. 
  halDigioIntClear(&pinGDO0);

}



//----------------------------------------------------------------------------------
//  void rxInitMode(uint8 mode, const HAL_RF_CONFIG *myRfConfig, const uint8 myPaTable[], const uint8 myPaTableLen)
//
//  DESCRIPTION:
//    Set up chip to operate in RX mode
//
//  ARGUMENTS:
//    uint8 mode                  - Indicate S-mode or T-mode
//    HAL_RF_CONFIG *myRfConfig   - Register config table
//    uint8 myPaTable             - PA table
//    uint8 uint8 myPaTableLen    - PA table length
//----------------------------------------------------------------------------------
void rxInitMode(uint8 mode, const HAL_RF_CONFIG *myRfConfig, const uint8 myPaTable[], const uint8 myPaTableLen)
{
  // Setup transceiver with the applied register settings
  halRfConfig(myRfConfig, myPaTable, myPaTableLen);
  
  // IDLE after TX and RX
  halRfWriteReg(CC2500_MCSM1, 0x00); 
  
  if (mode == WMBUS_SMODE)
  {
    // SYNC
    // In this application the 2 MSB of the 
    // Sync word is ignored. An alternative 
    // approach is to check the 8 LSB of the
    // syncword when reading the length byte
    halRfWriteReg(CC2500_SYNC1, 0x76); 
    halRfWriteReg(CC2500_SYNC0, 0x96); 
  }

  else
  {
    // SYNC
    halRfWriteReg(CC2500_SYNC1, 0x54); 
    halRfWriteReg(CC2500_SYNC0, 0x3D); 
  }

  // Set GDO0 to be RX FIFO threshold signal
  halRfWriteReg(CC2500_IOCFG0, 0x00);
  
  // Set up interrupt on GDO0
  halDigioIntSetEdge(&pinGDO0, HAL_DIGIO_INT_RISING_EDGE);
  halDigioIntConnect(&pinGDO0, &rxFifoISR);

  // Set GDO2 to be packet received signal
  halRfWriteReg(CC2500_IOCFG2, 0x06);

  // Set up interrupt on GDO2
  halDigioIntSetEdge(&pinGDO2, HAL_DIGIO_INT_FALLING_EDGE);
  halDigioIntConnect(&pinGDO2, &rxPacketRecvdISR);
 
}




//----------------------------------------------------------------------------------
//  uint16 rxRecvPacket(uint8* packet, uint8* bytes, uint8 mode)
//
//  DESCRIPTION:
//    Receive a Wireless MBUS packet from the radio. The function will strobe RX, and
//    "uses" the FIFO threshold ISR or packet received ISR and reads data from 
//    the RX FIFO accordingly. When all bytes are received the packet is decoded.
//  
//    Returns an error code if failure in received packet.
//
//  ARGUMENTS:
//    uint8 *packet   - Pointer to store the Wireless MBUS packet
//    uint8 *bytes    - Pointer to where to write the incoming bytes
//    uint8 mode      - Indicate S-mode or T-mode
//
//  RETURNS:
//    PACKET_OK                0
//    PACKET_CODING_ERROR      1
//    PACKET_CRC_MISMATCH      2
//    RX_STATE_ERROR           3
//----------------------------------------------------------------------------------
uint16 rxRecvPacket(uint8* packet, uint8* bytes, uint8 mode)
{ 

  // Set LPM pin to indicate active mode
  halDigioSet(&pinLPM);    
  
  uint16 rxStatus;
  uint16 key;
  
  // Initialize RX info variable
  RXinfo.lengthField = 0;           // Length Field in the wireless MBUS packet
  RXinfo.length      = 0;           // Total length of bytes to receive packet
  RXinfo.bytesLeft   = 0;           // Bytes left to to be read from the RX FIFO
  RXinfo.pByteIndex  = bytes;       // Pointer to current position in the byte array
  RXinfo.format      = INFINITE;    // Infinite or fixed packet mode
  RXinfo.start       = TRUE;        // Sync or End of Packet
  RXinfo.complete    = FALSE;       // Packet Received
  RXinfo.mode        = mode;        // Wireless MBUS mode           

 
  // Set RX FIFO threshold to 4 bytes
  halRfWriteReg(CC2500_FIFOTHR, RX_FIFO_START_THRESHOLD);

  // Set infinite length 
  halRfWriteReg(CC2500_PKTCTRL0, INFINITE_PACKET_LENGTH);


  // Check RX Status
  // Abort if not in IDLE
  rxStatus = halRfGetRxStatus();  
  if ( (rxStatus & CC2500_STATUS_STATE_BM) != CC2500_STATE_IDLE )
  {
    halRfStrobe(CC2500_SIDLE);
    return (RX_STATE_ERROR);
  }
  
  // Flush RX FIFO
  // Ensure that FIFO is empty before reception is started
  halRfStrobe(CC2500_SFRX);

  // Clear interrupt flags
  halDigioIntClear(&pinGDO2);
  halDigioIntClear(&pinGDO0);

  // Enable interrupt  
  halDigioIntEnable(&pinGDO2);
  halDigioIntEnable(&pinGDO0);
  
  // Strobe RX
  halRfStrobe(CC2500_SRX);

  // Critical code section
  // Interrupts must be disabled between checking for completion
  // and enabling low power mode. Else if RXinfo.complete is set after
  // the check, and before low power mode is entered the code will 
  // enter a dead lock.
  
  // Disable global interrupts
  HAL_INT_LOCK(key);  
  
  // Wait for FIFO being filled
  while (RXinfo.complete != TRUE)
  {
    // Clear LPM pin to indicate LPM mode
    halDigioClear(&pinLPM);
     
    // Set low power mode
    // Will Enable Interrupts
    halMcuSetLowPowerMode(HAL_MCU_LPM_3); 
    
    // Disable global interrupts
    HAL_INT_LOCK(key);  
  }
  
  // Set previous global interrupt state
  HAL_INT_UNLOCK(key); 
  
  // Disable interrupt  
  halDigioIntDisable(&pinGDO0);
  halDigioIntDisable(&pinGDO2);

  // Check that transceiver is in IDLE
  rxStatus = halRfGetRxStatus();  
  if ( (rxStatus & CC2500_STATUS_STATE_BM) != CC2500_STATE_IDLE )
  {
    halRfStrobe(CC2500_SIDLE);
    return (RX_STATE_ERROR);
  }

  // - Decode received bytes -
  // S-mode
  if (RXinfo.mode == WMBUS_SMODE)
    rxStatus = decodeRXBytesSmode(bytes, packet, packetSize(RXinfo.lengthField));
  // T-mode
  else 
    rxStatus = decodeRXBytesTmode(bytes, packet, packetSize(RXinfo.lengthField));

  
  return (rxStatus);
    
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

