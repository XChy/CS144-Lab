#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <optional>

using namespace std;

TCPReceiver::TCPReceiver() : ISN( nullopt ), FIN( false ) {}

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( message.SYN )
    ISN = message.seqno;

  if ( !ISN.has_value() )
    return;

  if ( message.FIN )
    FIN = true;

  reassembler.insert( message.seqno.unwrap( ISN.value(), reassembler.bytes_pending() ) + message.SYN - 1ll,
                      message.payload,
                      message.FIN,
                      inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  (void)inbound_stream;
  TCPReceiverMessage ret;
  if ( !ISN.has_value() )
    ret.ackno = nullopt;
  else
    // +1 for the SYN flag, and finish only when FIN flag reached and stream is closed.
    ret.ackno
      = Wrap32::wrap( inbound_stream.bytes_pushed() + 1 + ( FIN && inbound_stream.is_closed() ), ISN.value() );

  ret.window_size = min( inbound_stream.available_capacity(), (uint64_t)UINT16_MAX );

  return ret;
}
