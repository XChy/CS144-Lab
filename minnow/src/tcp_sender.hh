#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <cstdint>
#include <map>

struct MsgWithFlag
{
  TCPSenderMessage msg = {};
  bool sent = false;
};

struct Timer
{
  uint64_t RTO = 0;
  uint64_t current_time = 0;
  bool running = false;
};

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t cur_ackno_;
  uint64_t cur_window_;
  uint64_t in_flight_seqnos_;
  uint64_t n_retrans_;
  // ackno + window_size
  bool finished_;
  Timer timer_;
  std::map<uint64_t, MsgWithFlag> seqno_to_msg_;

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
private:
  std::string gen_payload( Reader& outbound_stream, uint64_t payload_length );

  void startTimer();
  void stopTimer();
};
