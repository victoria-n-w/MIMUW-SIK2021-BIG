
#include "ClientManager.h"
#include "../util/CRC.h"
#include "../util/err.h"
#include "../util/util.h"
#include "ClientState.h"
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

void ClientManager::start() {
  std::thread threads[] = {
      std::thread(&ClientManager::server_to_gui, this),
      std::thread(&ClientManager::gui_to_client, this),
      std::thread(&ClientManager::client_to_server, this),
      std::thread(&ClientState::dispatch_queue, &state, gui_socket),
  };
  for (auto &t : threads)
    t.join();
}

void ClientManager::server_to_gui() {
  while (true) {

    // read data from the server
    ssize_t size = recv(game_socket, server_buffer, BUFFER_SIZE, 0);
    if (size < 0)
      syserr("Could not read from server socket");

    // TODO czy na pewno jest rozłączane
    if (size == 0)
      break;

    parse_server_buffer(size);
  }
}

[[noreturn]] void ClientManager::gui_to_client() {
  while (true) {
    state.update_direction(read_from_gui());
  }
}

[[noreturn]] void ClientManager::client_to_server() {
  while (true) {
    auto wake_up_at = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(SEND_TO_SERVER_T);
    send_to_server();
    std::this_thread::sleep_until(wake_up_at);
  }
}
msg_from_gui ClientManager::read_from_gui() {
  unsigned char first_letter = gui_buffer.getchar();
  std::string pattern;

  if (first_letter == 'L') {
    pattern = "EFT_KEY_";

    if (!gui_buffer.match_pattern(pattern))
      return UNKNOWN;

    first_letter = gui_buffer.getchar();
    if (first_letter == 'U') {
      if (gui_buffer.getchar() == 'P')
        return LEFT_KEY_UP;
    }
    if (first_letter == 'D') {
      pattern = std::string("OWN");
      if (!gui_buffer.match_pattern(pattern))
        return UNKNOWN;
      return LEFT_KEY_DOWN;
    }

  } else if (first_letter == 'R') {
    pattern = "IGHT_KEY_";
    if (!gui_buffer.match_pattern(pattern))
      return UNKNOWN;

    first_letter = gui_buffer.getchar();
    if (first_letter == 'U') {
      if (gui_buffer.getchar() == 'P')
        return RIGHT_KEY_UP;
    }
    if (first_letter == 'D') {
      pattern = std::string("OWN");
      if (!gui_buffer.match_pattern(pattern))
        return UNKNOWN;
      return RIGHT_KEY_DOWN;
    }
  }

  return UNKNOWN;
}
void ClientManager::send_to_server() {
  std::vector<unsigned char> message;
  // TODO change htonl to 64 bit
  message.push_back(htonl(state.get_session_id()));
  message.push_back(htons(state.get_turn_direction()));
  message.push_back(htonl(state.get_next_expected_event_no()));
  std::copy(state.get_name().begin(), state.get_name().end(),
            std::back_inserter(message));

  if (sendto(game_socket, message.data(), message.size(), 0,
             server_addr->ai_addr, server_addr->ai_addrlen) < 0)
    syserr("Could not send datagram to server");
}

ClientManager::~ClientManager() {
  if (close(game_socket) < 0)
    syserr("Cannot close game socket");

  if (close(gui_socket) < 0)
    syserr("Cannot close gui socket");
}

void ClientManager::parse_server_buffer(ssize_t size) {
  uint32_t game_number = util::read_uint32_from_network_stream(server_buffer);
  if (!state.valid_game_number(game_number))
    return;

  ssize_t counter = GAME_NO_BYTES;

  bool crc_ok = true;
  while (counter < size && crc_ok)
    parse_event(counter, size, crc_ok);
}

void ClientManager::parse_event(ssize_t &counter, ssize_t size, bool &crc_ok) {
  uint32_t len =
      ntohl(*(reinterpret_cast<uint32_t *>(server_buffer + counter)));
  if (len + CRC32LEN + LEN_LEN >= BUFFER_SIZE - counter) {
    // len is invalid, as it doesn't fit in the buffer
    counter = size;
    return;
  }

  uint32_t crc = CRC::calculate(server_buffer + counter,
                                server_buffer + counter + LEN_LEN + len);
  uint32_t crc_received = ntohl(
      *(reinterpret_cast<uint32_t *>(server_buffer + counter + LEN_LEN + len)));

  if (crc != crc_received) {
    // crc is invalid
    crc_ok = false;
    return;
  }

  if (len < MINIMUM_EVENT__LEN) {
    // data has no sense
    fatal("Received event makes no sense - it's length is too short");
  }

  counter += LEN_LEN;

  uint32_t event_no =
      ntohl(*(reinterpret_cast<uint32_t *>(server_buffer + counter)));

  state.set_next_expected_event_no(event_no + 1);

  ssize_t end = counter + len;
  counter += 4;

  switch (server_buffer[counter++]) {
  case NEW_GAME_EVENT:
    parse_new_game_event(counter, end, event_no);
    break;
  case PIXEL_EVENT:
    parse_pixel_event(counter, end, event_no);
    break;
  case PLAYER_ELIMINATED_EVENT:
    parse_player_eliminated_event(counter, end, event_no);
    break;
  case GAME_OVER_EVENT:
    handle_game_over_event(event_no);
  }
  counter = end + CRC32LEN;
}

/**
 * Adds new event to the event queue.
 * Changes the state
 * @param start index of the start of the event_data field
 * @param end index of the end of the event_data_field
 */
void ClientManager::parse_new_game_event(ssize_t start, ssize_t end,
                                         uint32_t event_no) {
  state.play_game(); // TODO numer gry
  uint32_t max_x = util::read_uint32_from_network_stream(server_buffer + start);
  start += 4;
  uint32_t max_y = util::read_uint32_from_network_stream(server_buffer + start);
  start += 4;

  // TODO a co z danymi bez sensu
  std::string name;
  while (server_buffer[start] != '\0' && start < end) {
    for (; start < end && server_buffer[start] != ' ' &&
           server_buffer[start] != '\0';
         ++start) {
      name.push_back(server_buffer[start]);
    }
    if (util::is_name_valid(name) && !name.empty()) {
      state.new_player(name);
    }
  }

  auto *data = new std::string{"NEW_GAME "};
  data->append(std::to_string(max_x));
  data->push_back(' ');
  data->append(std::to_string(max_y));

  state.append_player_names(*data);
  state.add_event(event_no, {NEW_GAME_EVENT, data});
}
void ClientManager::parse_pixel_event(ssize_t start, ssize_t end,
                                      uint32_t event_no) {
  if (start - end != PIXEL_EVENT_LEN)
    fatal("Invalid data in PIXEL event");
  // TODO validate data
  uint32_t x, y;
  unsigned char player_no = server_buffer[start++];
  x = util::read_uint32_from_network_stream(server_buffer + start);
  start += 4;
  y = util::read_uint32_from_network_stream(server_buffer + start);
  auto data = new std::string("PIXEL ");
  data->append(std::to_string(x) + ' ');
  data->append(std::to_string(y) + ' ');
  data->append(state.get_player_name(player_no));

  state.add_event(event_no, {PIXEL_EVENT, data});
}
void ClientManager::parse_player_eliminated_event(ssize_t start, ssize_t end,
                                                  uint32_t event_no) {

  if (start - end != PLAYER_ELIMINATED_EVENT_LEN)
    fatal("Invalid data in PLAYER_ELIMINATED event");

  // TODO validate data
  auto data = new std::string("PLAYER_ELIMINATED ");
  data->append(state.get_player_name(server_buffer[start]));

  state.add_event(event_no, {PLAYER_ELIMINATED_EVENT, data});
}
void ClientManager::handle_game_over_event(uint32_t event_no) {
  // TODO validate data
  auto data = new std::string();
  state.add_event(event_no, {GAME_OVER_EVENT, data});
}