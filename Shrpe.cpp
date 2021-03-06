/*
  Shrpe.cpp - Library for Arduino Shrpe shield.
  Copyright (c) 2011-2014 Arduino LLC.  All right reserved.
	Authors: Andreas Drotth & Soded Alatia, 
  Modified: July 23, 2015.
  Release version: v3.0

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "Shrpe.h"
#include "packet_framing_library/CRC_16.cpp"
#include "packet_framing_library/Framing.cpp"
#include "packet_framing_library/Timer.cpp"

#define HWREV_2

Framing framing;
byte input_buff[100], upl_obj_ack_buff[2], dl_obj_buff[38];
byte intern_dl_buf[38], intern_upl_buf[40];
int input_length, crc_valid, result, timer_counter;
size_t dl_obj_len, intern_upl_len, intern_dl_len, ack_len;
static int reset_event = -1;
bool reset_event_rxed = false;

Shrpe::Shrpe()
{
	// Driver initialization
    _state = SHRPE_STATE_UNKNOWN;
		intern_dl_len = 38;
		intern_upl_len = 0;
		timer_counter = 1000;
}

int Shrpe::begin()
{
	// wait for reset event
  // reset shield
	Serial.begin(115200);
  framing.setTimout(0.1);
  pinMode(2, INPUT); // IRQ-PIN
  digitalWrite(2, HIGH); // turn on pull-up resistor
  // wait for first state change event
	
#ifdef HWREV_2
	/*
	*	Since there's a physical jumper that must be in place
	* in order to have a working communication between the 
	*	Arduino board & shield, this reset-method is easier.
	*/
	while(reset_event < 0) {
		reset_event = resetShield();
		delay(2000);
	}
	return SHRPE_OK;
#else
	
	do {
		loop();
		//a minimum of 1ms delay for the loop execution time.
		delay(1);
	}
  while(reset_event < 0 && timer_counter--);
	if(!reset_event_rxed) {
		resetShield();
		Serial.println("timer timeout, sent reset msg");
	}
	Serial.print("Timer_counter: ");
	Serial.println(timer_counter);
  return _state;
#endif
}

void Shrpe::loop()
{
  //while pending message in shield
	
  while (!digitalRead(2)) {
		int msg[39];
		size_t length;
    if(getNextMessage(msg, &length) == SHRPE_OK) {
			switch (msg[0]) {
			case SHRPE_RESET_EVENT:
				reset_event = msg[0];
				_state = SHRPE_STATE_CONNECTING;
				reset_event_rxed = true;
				Serial.println("RESET EVENT RECEIVED. state = CONNECTING");
      case SHRPE_STATE_CHANGED:
        _state = (shrpe_state_t) msg[1];
        break;
      case SHRPE_UPLOAD_OBJECT_ACK:
				upl_obj_ack_buff[0] = 1;
				upl_obj_ack_buff[1] = msg[1];
				ack_len = length-1;
        break;
      case SHRPE_DOWNLOAD_OBJECT:
				for(int i = 0; i < length-1; i++) {
					dl_obj_buff[i] = msg[i+1];
				}
				dl_obj_len = length-1;
        break;
      default:
        break;
			}
		}
  }
}

shrpe_result_t Shrpe::getNextMessage(int* msg, size_t *length)
{
	uint8_t data_array[1] = {SHRPE_GET_NEXT_MSG};
  if (framing.sendFramedData(data_array, 1)) {
  
    framing.receiveFramedData(input_buff, input_length, crc_valid);
    if(crc_valid == 1) {
	  for (int i = 0; i < input_length; i++){
	    msg[i] = input_buff[i];
	  }
			*length = input_length;
			return SHRPE_OK;  
    }
		return SHRPE_ERR_CRC;
  }
  return SHRPE_ERR_TIMEOUT;
}

int Shrpe::sendUploadObject(const uint8_t *buffer, size_t size)
{
	if(_state < SHRPE_STATE_CONNECTING)
		return SHRPE_STATE_ILLEGAL;
  // send UploadObject command
  int result = size;
	uint8_t msg[size+1];
	msg[0] = SHRPE_UPLOAD_OBJECT;
	for(int i = 0; i < size; i++) {
		msg[i+1] = buffer[i];
	}
	if(framing.sendFramedData(msg, size+1)) {
		framing.receiveFramedData(input_buff, input_length, crc_valid);
		if (crc_valid == 1)  {
			return input_buff[0];
		}
		return SHRPE_ERR_CRC;
	}
  return SHRPE_ERR_TIMEOUT;
}

int Shrpe::receiveUploadObjectAck(uint8_t *buffer, size_t length)
{
  // check if UploadObjectAck is received
  // for now assume that an ack is received
  //buffer[0] = 1; // 1 => ack is received, 0 => tx failed, no ack received
  //buffer[1] = 0; // ack payload if ack received OR error code if tx failed
  //result = upl_obj_ack_buff[0];
	if(length < 2) {
		return SHRPE_ERR_LEN;
	}
	if(upl_obj_ack_buff[1] == 0) {
		*buffer = upl_obj_ack_buff[0];
		*(buffer+1) = upl_obj_ack_buff[1];
		upl_obj_ack_buff[0] = 0;
		return ack_len;
	} else {
		*buffer = 0;
		*(buffer+1) = upl_obj_ack_buff[1];
		upl_obj_ack_buff[0] = 0;
		return ack_len;
	}
	return SHRPE_ERR_UNDEFINED;
}

int Shrpe::receiveDownloadObject(uint8_t *buffer, size_t length)
{
	for(int i = 0; i < length; i++) {
		*(buffer+i) = dl_obj_buff[i];
	}
	int len = dl_obj_len;
	for(int i = 0; i < dl_obj_len; i++) {
		dl_obj_buff[i] = NULL;
	}
	dl_obj_len = 0;
  return len;
}

int Shrpe::setContacts(uint8_t contacts)
{
	// send UploadObject command
	uint8_t msg[2];
	msg[0] = SHRPE_SET_CONTACTS;
	msg[1] = contacts;
	if(framing.sendFramedData(msg, 2)) {
		framing.receiveFramedData(input_buff, input_length, crc_valid);
		if (crc_valid == 1)  {
			return input_buff[0];
		}
		return SHRPE_ERR_CRC;
	}
  return SHRPE_ERR_TIMEOUT;
}

int Shrpe::resetShield(void)
{
	// send UploadObject command
	uint8_t msg[1];
	msg[0] = SHRPE_RESET_SHIELD;
	if(framing.sendFramedData(msg, 1)) {
		framing.receiveFramedData(input_buff, input_length, crc_valid);
		if (crc_valid == 1)  {
			return input_buff[0];
		}
		return SHRPE_ERR_CRC;
	}
  return SHRPE_ERR_TIMEOUT;
}

int Shrpe::available()
{
	/*
	Serial.print("dl_obj_len: ");
	Serial.println(dl_obj_len);
	
	Serial.print("sizeof(arr): ");
	Serial.println(sizeof(dl_obj_buff));
	
	Serial.print("sizeof(arr)/sizeof(arr[0]): ");
	Serial.println(sizeof(dl_obj_buff)/sizeof(dl_obj_buff[0]));
	*/
  return (dl_obj_len);
}

int Shrpe::read()
{
	if(!available()) {
		receiveDownloadObject(intern_dl_buf, intern_dl_len);
	}
	if(available()) {
		int byte = dl_obj_buff[0];
		for(int i = 1; i <= available(); i++) {
			dl_obj_buff[i-1] = dl_obj_buff[i];
		}
		dl_obj_len--;
		return byte;
	}
	return -1;
}

int Shrpe::peek()
{
  if(available()) {
		return dl_obj_buff[0];
	}
	return -1;
}

void Shrpe::flush()
{
	for(int i = 0; i < intern_dl_len; i++) {
		intern_dl_buf[i] = NULL;
	}
	intern_dl_len = 0;
}

size_t Shrpe::write(const uint8_t byte)
{
	if(intern_upl_len < 40) {
		intern_upl_buf[intern_upl_len++] = byte;
		return 1;
	}
  return 0;
}

size_t Shrpe::write(const uint8_t *buffer, size_t size)
{
	if(size > 40) {
		size = 40;
	}
	intern_upl_len = size;
	for(int i = 0; i < size; i++) {
		intern_upl_buf[i] = *(buffer+i);
	}
	return intern_upl_len;
}

int Shrpe::flushWriteBuffer()
{
	int result = sendUploadObject(intern_upl_buf, intern_upl_len);
	if(result == intern_upl_len || result == 40) {
		for(int i = 0; i < intern_dl_len; i++) {
			intern_upl_buf[i] = NULL;
		}
		intern_upl_len = 0;
		return 0;
	}
  return SHRPE_ERR_LEN;
}
