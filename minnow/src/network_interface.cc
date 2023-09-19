#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , time_( 0 )
  , frames_()
  , ip_to_ethernet_()
  , ip_to_time_()
  , ips_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

void NetworkInterface::pushARP( uint16_t opcode, uint32_t target_ip, EthernetAddress target_eth )
{
  Serializer serializer;
  EthernetHeader header;
  header.type = EthernetHeader::TYPE_ARP;
  header.src = ethernet_address_;
  header.dst = target_eth;

  ARPMessage payload;
  payload.opcode = opcode;
  payload.sender_ethernet_address = ethernet_address_;
  payload.sender_ip_address = ip_address_.ipv4_numeric();
  if ( target_eth == boardcast_address ) {
    payload.target_ethernet_address = {};
  } else {
    payload.target_ethernet_address = target_eth;
  }
  payload.target_ip_address = target_ip;
  header.serialize( serializer );
  payload.serialize( serializer );

  Parser parser( serializer.output() );

  EthernetFrame frame_to_send;
  frame_to_send.parse( parser );
  frames_.push_front( frame_to_send );
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  int32_t hop_addr = next_hop.ipv4_numeric();

  // ARP Request
  if ( !ip_to_ethernet_.contains( hop_addr ) ) {
    pushARP( ARPMessage::OPCODE_REQUEST, next_hop.ipv4_numeric(), boardcast_address );
    ip_to_ethernet_[hop_addr] = boardcast_address;
    ip_to_time_[hop_addr] = time_;
  }

  // Queue the IP datagram
  EthernetHeader header;
  Serializer serializer;
  header.src = ethernet_address_;
  // destination ethernet address will be filled when sending
  header.type = EthernetHeader::TYPE_IPv4;

  header.serialize( serializer );
  dgram.serialize( serializer );

  Parser parser( serializer.output() );
  EthernetFrame frame;
  frame.parse( parser );
  frames_.push_back( frame );

  ips_.push_back( hop_addr );
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // If not inbound, ignore it
  if ( frame.header.dst != boardcast_address && frame.header.dst != ethernet_address_ )
    return {};

  Parser parser( frame.payload );
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp;
    arp.parse( parser );
    // If not inbound, ignore it
    if ( parser.has_error() || arp.target_ip_address != ip_address_.ipv4_numeric() )
      return {};

    // If inbound, remember it
    ip_to_ethernet_[arp.sender_ip_address] = arp.sender_ethernet_address;
    if ( !ip_to_time_.contains( arp.sender_ip_address ) )
      ip_to_time_[arp.sender_ip_address] = time_;

    if ( arp.opcode == ARPMessage::OPCODE_REQUEST )
      pushARP( ARPMessage::OPCODE_REPLY, arp.sender_ip_address, frame.header.src );

  } else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram datg;
    datg.parse( parser );

    if ( parser.has_error() )
      return {};

    return datg;
  }
  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  time_ += ms_since_last_tick;
  for ( auto& [ip, t] : ip_to_time_ ) {
    // Resend ARP requests if timeout
    if ( ip_to_ethernet_[ip] == boardcast_address && time_ - t >= 5000 ) {
      pushARP( ARPMessage::OPCODE_REQUEST, ip, boardcast_address );
      ip_to_time_[ip] = time_;
    } else if ( time_ - t >= 30000 ) {
      ip_to_ethernet_.erase( ip );
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( frames_.empty() )
    return {};

  EthernetFrame frame = frames_.front();
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    frames_.pop_front();
    return frame;
  } else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    if ( !ip_to_ethernet_.contains( ips_.front() ) || ip_to_ethernet_[ips_.front()] == boardcast_address )
      return {};

    frame.header.dst = ip_to_ethernet_[ips_.front()];
    ips_.pop_front();
    frames_.pop_front();
    return frame;
  }

  return {};
}
