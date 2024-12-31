#ifndef CAN_BUS_H
#define CAN_BUS_H

void handleCANBus();
void sendCAN_LE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4);
void sendCAN_BE(int CANaddress, int inputVal_1, int inputVal_2, int inputVal_3, int inputVal_4);
void receiveCAN();
void parseCAN(unsigned long id, unsigned long msg);

#endif // CAN_BUS_H