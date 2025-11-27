/***********************************************************************************
    Filename: tx.c
***********************************************************************************/

#include <hal_types.h>
#include <hal_defs.h>
#include <hal_mcu.h>
#include <hal_int.h>
#include <hal_board.h>
#include <hal_rf.h>
#include <cc2500.h>
#include <mbus_packet.h>
#include <tx.h>


//----------------------------------------------------------------------------------
//  Note:
//  The CC2500 RF register map is identical to the CC1101 RF register map.
//----------------------------------------------------------------------------------

//----------------------------------------------------------------------------------
//  Variables used in this file
//----------------------------------------------------------------------------------
TXinfoDescr TXinfo;
extern const digioConfig pinLPM;


//----------------------------------------------------------------------------------
//  void txFifoISR(void)
//
//  DESCRIPTION:
//    This function is called when the FIFO Threshold signal is deasserted, indicating
//    that the FIFO (in this example) is half full. 
//    The TX FIFO is filled with new data, and the transceiver set to FIXED mode if 
//    less than 256 bytes left to transmit
//----------------------------------------------------------------------------------

static void txFifoISR(void)
{
  uint8  bytesToWrite;
    
  // Set LPM pin to indicate active mode
  halDigioSet(&pinLPM);     
    
  // Write data fragment to TX FIFO
  bytesToWrite = MIN(TX_AVAILABLE_FIFO, TXinfo.bytesLeft);
  halRfWriteFifo(TXinfo.pByteIndex, bytesToWrite);
  
  TXinfo.pByteIndex   += bytesToWrite;
  TXinfo.bytesLeft    -= bytesToWrite;

  // Indicate complete when all bytes are written to TX FIFO
  if (!TXinfo.bytesLeft)
    TXinfo.complete = TRUE; 

  // Set Fixed length mode if less than 256 left to transmit
  if ((TXinfo.bytesLeft < (MAX_FIXED_LENGTH - TX_FIFO_SIZE)) && (TXinfo.format == INFINITE))
  {
    halRfWriteReg(CC2500_PKTCTRL0, FIXED_PACKET_LENGTH);
    TXinfo.format = FIXED;
  }
    
  // We don't want to handle a potential pending TX FIFO interrupt immediately now. 
  // The TX FIFO interrupt says that the FIFO is half full, however we have just
  // written to the FIFO
  halDigioIntClear(&pinGDO0);
}



//----------------------------------------------------------------------------------
//  void txInitMode(uint8 mode, const HAL_RF_CONFIG *myRfConfig, const uint8 myPaTable[], const uint8 myPaTableLen)
//
//  DESCRIPTION:
//    Set up chip to operate in TX
// 
//  ARGUMENTS:
//    uint8 mode                  - Indicate S-mode or T-mode
//    HAL_RF_CONFIG *myRfConfig   - Register config table
//    uint8 myPaTable             - PA table
//    uint8 uint8 myPaTableLen    - PA table length
//----------------------------------------------------------------------------------

void txInitMode(uint8 mode, const HAL_RF_CONFIG *myRfConfig, const uint8 myPaTable[], const uint8 myPaTableLen)
{
 
  // Setup tranceiver with applied register settings
  halRfConfig(myRfConfig, myPaTable, myPaTableLen);
  
  // IDLE after TX and RX
  halRfWriteReg(CC2500_MCSM1, 0x00); 
  
  // Set FIFO threshold
  halRfWriteReg(CC2500_FIFOTHR, TX_FIFO_THRESHOLD);

  if (mode == WMBUS_SMODE)
  {
    // SYNC
    // The TX FIFO must apply the last byte of the 
    // Synchronization word
    halRfWriteReg(CC2500_SYNC1, 0x54); 
    halRfWriteReg(CC2500_SYNC0, 0x76); 
  }

  else
  {
    // SYNC
    halRfWriteReg(CC2500_SYNC1, 0x54); 
    halRfWriteReg(CC2500_SYNC0, 0x3D);
    
    // Set Deviation to 50 kHz
    halRfWriteReg(CC2500_DEVIATN, 0x50);
    
    // Set data rate to 100 kbaud
    halRfWriteReg(CC2500_MDMCFG4, 0x5B);
    halRfWriteReg(CC2500_MDMCFG3, 0xF8);
    
  }

  // Set GDO0 to be TX FIFO threshold signal
  halRfWriteReg(CC2500_IOCFG0, 0x02);

  // Set up txFifoISR interrupt
  halDigioIntSetEdge(&pinGDO0, HAL_DIGIO_INT_FALLING_EDGE);
  halDigioIntConnect(&pinGDO0, &txFifoISR);

}



//----------------------------------------------------------------------------------
//  uint16 txSendPacket(uint8* pPacket, uint8* pBytes, uint8 mode)
//
//  DESCRIPTION:
//    Send a packet over the air. Use flow control features of the CC1101
//    to regulate the number of bytes that can be written to the FIFO at one time.
//    Return once the packet has been written to the FIFO (i.e. don't wait for the
//    packet to actually be sent).
//
//  ARGUMENTS:
//    uint8 *pPacket        - Pointer to the Wirless MBUS packet
//    uint8 *pBytes         - Pointer to where to store the encoded packet
//    uint8 mode            - Indicate S-mode or T-mode
//
//  RETURNS:
//    TX_OK                 - TX OK
//    TX_LENGTH_ERROR       - Illegal length of packet 
//    TX_STATE_ERROR        - TX in illegal state
//----------------------------------------------------------------------------------

uint16 txSendPacket(uint8* pPacket, uint8* pBytes, uint8 mode)
{  
  uint16  bytesToWrite;
  uint16  fixedLength;
  uint8   txStatus;
  uint16  packetLength;
  uint16  key;
  
  // Set LPM pin to indicate active mode
  halDigioSet(&pinLPM);     
  
  // Calculate total number of bytes in the wireless MBUS packet
  packetLength = packetSize(pPacket[0]);
   
  // Check for valid length
  if ((packetLength == 0) || (packetLength > 290))
    return TX_LENGTH_ERROR;
  
  
  // - Data encode packet and calculate number of bytes to transmit 
  // S-mode
  if (mode == WMBUS_SMODE)
  {
    encodeTXBytesSmode(pBytes, pPacket, packetLength);
    TXinfo.bytesLeft = byteSize(1, 1, packetLength);   
  }
  
  // T-mode
  else
  {
    encodeTXBytesTmode(pBytes, pPacket, packetLength);
    TXinfo.bytesLeft = byteSize(0, 1, packetLength);   
  }
  
  // Check TX Status
  txStatus = halRfGetTxStatus();  
  if ( (txStatus & CC2500_STATUS_STATE_BM) != CC2500_STATE_IDLE )
  {
    halRfStrobe(CC2500_SIDLE); 
    return TX_STATE_ERROR;
  }
  
  // Flush TX FIFO 
  // Ensure that FIFO is empty before transmit is started
  halRfStrobe(CC2500_SFTX);
  
  // Initialize the TXinfo struct.
  TXinfo.pByteIndex   = pBytes;  
  TXinfo.complete     = FALSE;      
      
  // Set fixed packet length mode if less than 256 bytes to transmit
  if (TXinfo.bytesLeft < (MAX_FIXED_LENGTH) )
  {
    fixedLength = TXinfo.bytesLeft;
    halRfWriteReg(CC2500_PKTLEN, (uint8)(TXinfo.bytesLeft));
    halRfWriteReg(CC2500_PKTCTRL0, FIXED_PACKET_LENGTH);
    TXinfo.format = FIXED;
  } 
    
  // Else set infinite length mode
  else 
  {
    fixedLength = TXinfo.bytesLeft % (MAX_FIXED_LENGTH);
    halRfWriteReg(CC2500_PKTLEN, (uint8)fixedLength);    
    halRfWriteReg(CC2500_PKTCTRL0, INFINITE_PACKET_LENGTH);
    TXinfo.format = INFINITE;
  }
 
  // Fill TX FIFO
  bytesToWrite = MIN(TX_FIFO_SIZE, TXinfo.bytesLeft);  
  halRfWriteFifo(TXinfo.pByteIndex, bytesToWrite);
  TXinfo.pByteIndex += bytesToWrite;
  TXinfo.bytesLeft  -= bytesToWrite;

  // Check for completion
  if (!TXinfo.bytesLeft)
    TXinfo.complete = TRUE; 
  
  // Set interrupt state, and enable TX
  // Safe to set states, as radio is IDLE
  halDigioIntClear(&pinGDO0);
  halDigioIntEnable(&pinGDO0);
  
  // Strobe TX
  halRfStrobe(CC2500_STX);
    
  // Critical code section
  // Interrupts must be disabled between checking for completion
  // and enabling low power mode. Else if TXinfo.complete is set after
  // the check, and before low power mode is entered the code will 
  // enter a dead lock.

  // Disable global interrupts
  HAL_INT_LOCK(key);  
  
  // Wait for available space in FIFO
  while (!TXinfo.complete)
  {
    // Check TX Status
    txStatus = halRfGetTxStatus();
    if ( (txStatus & CC2500_STATUS_STATE_BM) == CC2500_STATE_TX_UNDERFLOW )
    {
      halRfStrobe(CC2500_SFTX);
      return TX_STATE_ERROR;
    }
    
    // Clear LPM pin to indicate LPM mode  
    halDigioClear(&pinLPM);
      
    // Set Low power mode 
    // Will Enable Interrupts
    halMcuSetLowPowerMode(HAL_MCU_LPM_3); 
    
    // Disable global interrupts
    HAL_INT_LOCK(key);  
   }
 
  // Set prevois global interrupt state
  HAL_INT_UNLOCK(key);  
  
  // Disable TX interrupt
  halDigioIntDisable(&pinGDO0);
    
  return (TX_OK);
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


