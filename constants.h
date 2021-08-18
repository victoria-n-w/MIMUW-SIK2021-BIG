#ifndef CONSTANTS_H
#define CONSTANTS_H

#define SEND_TO_SERVER_T 30

#define BUFFER_SIZE 4096

#define LEFT_DIRECTION 2
#define STRAIGHT 0
#define RIGHT_DIRECTION 1

enum msg_from_gui {
  LEFT_KEY_UP,
  LEFT_KEY_DOWN,
  RIGHT_KEY_UP,
  RIGHT_KEY_DOWN,
  UNKNOWN,
};

#endif // CONSTANTS_H
