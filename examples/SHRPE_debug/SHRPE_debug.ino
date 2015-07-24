/*
  Looping Write with SHRPE

  A simple example of how to use the 'write' and 'read' functionality
  with the SHRPE library. The program loops forever,
  continuously sending an array of 40 bytes, every 5th second.
  If there is data available (sent from the gateway), it will read
  the data and send it back.

  Notice the shrpe.begin() call in the setup function.
  This function is necessary for the Shrpe object to function
  properly.

  Created 30 June 2015
  By Andreas Drotth & Soded Alatia
*/

#include <Shrpe.h>
#include <SoftwareSerial.h>

Shrpe shrpe;
SoftwareSerial mySerial(10, 11); // RX, TX
uint8_t counter = 0;
int result = 0;
uint8_t array[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                   11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                   21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
                   31, 32, 33, 34, 35, 36, 37, 38, 39, 40
                  };
uint8_t event_ack[2];
uint8_t STATE = 0;
uint8_t CONNECTED = 32;

void setup()
{
  shrpe.begin();
  mySerial.begin(115200);
  mySerial.println("\nStarting arduino shrpe program");
}

void loop()
{
  if (STATE != CONNECTED) {
    //delay(2000);
    //result = shrpe.write(0, MSG_GET_STATE);
    while (!shrpe.available());
    int len = shrpe.read(event_ack, 2);
    mySerial.print("\nLength of event_ack: ");
    mySerial.println(len);
    mySerial.print("MSG TYPE: ");
    mySerial.println(event_ack[0]);
    mySerial.print("ACK RESULT: ");
    mySerial.println(event_ack[1]);
    mySerial.println("Waiting for Connection...");
    STATE = event_ack[1];
  } else {
    if (++counter % 5 == 0) {
      result = shrpe.write(array, sizeof(array), MSG_UPLOAD_OBJECT);
      mySerial.println("\nUploading 1 object: 40 bytes");
      if (result == 0) {
        mySerial.print("write result of first object: ");
        mySerial.println(result);
        while (!shrpe.available());
        int len = shrpe.read(event_ack, 2);
        mySerial.print("Length of event_ack: ");
        mySerial.println(len);
        mySerial.print("MSG TYPE: ");
        mySerial.println(event_ack[0]);
        mySerial.print("ACK RESULT: ");
        mySerial.println(event_ack[1]);
      } else {
        mySerial.println("Error sending array object");
      }
    }

    if (shrpe.available()) {
      uint8_t incoming_data[38];
      int len;
      len = shrpe.read(incoming_data, 38);
      mySerial.print("downloading object of length: ");
      mySerial.println(len);
    }
  }
  delay(1000);
}
