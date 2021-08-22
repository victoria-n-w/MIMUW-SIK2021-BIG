#ifndef CONSTANTS_H
#define CONSTANTS_H

#define SEND_TO_SERVER_T 30

#define BUFFER_SIZE 4096
#define GAME_NO_BYTES 4
#define LEN_LEN 4
#define MINIMUM_EVENT__LEN 5
#define CRC32LEN 4

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

// event types
#define NEW_GAME_EVENT 0
#define PIXEL_EVENT 1
#define PLAYER_ELIMINATED_EVENT 2
#define GAME_OVER_EVENT 3

// lenghts of messages from server
#define PIXEL_EVENT_LEN 9
#define PLAYER_ELIMINATED_EVENT_LEN 1
#define GAME_OVER_LEN 0

#define MAX_CLIENT_DATAGRAM_LEN 33

#endif // CONSTANTS_H