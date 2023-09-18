#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <optional>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , cur_ackno_( 0 )
  , cur_window_( 1 )
  , in_flight_seqnos_( 0 )
  , n_retrans_( 0 )
  , finished_( false )
  , timer_()
  , seqno_to_msg_()
{
  timer_.RTO = initial_RTO_ms;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  if ( seqno_to_msg_.empty() )
    return 0;
  else {
    uint64_t begin = seqno_to_msg_.begin()->first;
    uint64_t end = ( --seqno_to_msg_.end() )->first + ( --seqno_to_msg_.end() )->second.msg.sequence_length();
    return end - begin;
  }
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return n_retrans_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // NOTE: Resend the earliest segment if timeout
  if ( seqno_to_msg_.empty() )
    return nullopt;

  for ( auto& [_, msg_sent] : seqno_to_msg_ ) {
    if ( !msg_sent.sent ) {
      if ( !timer_.running )
        startTimer();

      msg_sent.sent = true;
      return msg_sent.msg;
    }
  }

  return nullopt;
}

void TCPSender::push( Reader& outbound_stream )
{
  uint64_t window = ( cur_window_ == 0 ? 1 : cur_window_ );
  uint64_t remaining = window > sequence_numbers_in_flight() ? window - sequence_numbers_in_flight() : 0;

  if ( finished_ )
    return;

  // If the stream has closed before popping, transmit FIN directly with available window.
  if ( outbound_stream.is_finished() && remaining >= 1 ) {
    auto msg = TCPSenderMessage { .seqno = Wrap32::wrap( cur_ackno_, isn_ ), .SYN = cur_ackno_ == 0, .FIN = true };
    seqno_to_msg_[cur_ackno_] = MsgWithFlag { .msg = msg, .sent = false };
    cur_ackno_ += msg.sequence_length();
    finished_ = true;
    return;
  }

  while ( remaining ) {
    bool SYN = cur_ackno_ == 0;

    uint64_t msg_length
      = min( min( remaining, TCPConfig::MAX_PAYLOAD_SIZE + SYN ), outbound_stream.bytes_buffered() + SYN );

    if ( msg_length == 0 )
      break;

    string payload = gen_payload( outbound_stream, msg_length - SYN );

    // If stream has been closed after popping and there is still one more available space for FIN ,
    // then we insert FIN
    bool FIN = outbound_stream.is_finished() && remaining - msg_length >= 1;
    if ( FIN )
      finished_ = true;
    TCPSenderMessage msg
      = TCPSenderMessage { .seqno = Wrap32::wrap( cur_ackno_, isn_ ), .SYN = SYN, .payload = payload, .FIN = FIN };
    msg_length += FIN;

    seqno_to_msg_[cur_ackno_] = MsgWithFlag { .msg = msg, .sent = false };
    cur_ackno_ += msg_length;
    remaining -= msg_length;
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage { .seqno = Wrap32::wrap( cur_ackno_, isn_ ) };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  cur_window_ = msg.window_size;

  // Remove any that have now been fully acknowledged
  int num_erased = 0;
  if ( msg.ackno.has_value() && !seqno_to_msg_.empty() ) {
    uint64_t abs_recv_seqno = msg.ackno.value().unwrap( isn_, cur_ackno_ );
    uint64_t end = ( --seqno_to_msg_.end() )->first + ( --seqno_to_msg_.end() )->second.msg.sequence_length();

    // ignore impossible ackno
    if ( abs_recv_seqno > end )
      return;

    num_erased = erase_if( seqno_to_msg_, [&]( auto p ) {
      TCPSenderMessage sender_msg = p.second.msg;
      uint64_t abs_sender_seqno_max
        = ( sender_msg.seqno + sender_msg.sequence_length() ).unwrap( isn_, cur_ackno_ );
      return abs_sender_seqno_max <= abs_recv_seqno;
    } );
  }

  // When all outstanding data has been acknowledged, stop the retransmission timer
  if ( num_erased ) {
    stopTimer();
    timer_.RTO = initial_RTO_ms_;

    if ( !seqno_to_msg_.empty() )
      startTimer();

    n_retrans_ = 0;
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if ( timer_.running ) {
    timer_.current_time += ms_since_last_tick;
  }

  if ( timer_.current_time >= timer_.RTO ) {
    // Retransmit the earliest (lowest sequence number) segment that hasn’t been fully acknowledged
    if ( !seqno_to_msg_.empty() )
      seqno_to_msg_.begin()->second.sent = false;

    if ( cur_window_ != 0 ) {
      // Keep track of consecutive retransmissions
      n_retrans_++;
      // Double RTO, "exponential backof"
      timer_.RTO *= 2;
    }

    //  Reset timer and start it
    stopTimer();
    startTimer();
  }
}

string TCPSender::gen_payload( Reader& outbound_stream, uint64_t payload_length )
{
  string payload;
  // NOTE: Such approach is inefficient but general for all kinds of bytestream
  while ( payload.size() < payload_length ) {
    string peeked = string( outbound_stream.peek().substr( 0, payload_length - payload.size() ) );
    payload.append( peeked );
    outbound_stream.pop( peeked.size() );
  }
  return payload;
}

void TCPSender::startTimer()
{
  timer_.running = true;
  timer_.current_time = 0;
}

void TCPSender::stopTimer()
{
  timer_.running = false;
}
