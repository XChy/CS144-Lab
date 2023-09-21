#include "router.hh"
#include "ipv4_datagram.hh"

#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  entries_.push_back( { route_prefix, prefix_length, next_hop, interface_num } );
}

optional<RouteEntry> findEntry( vector<RouteEntry>& entries, uint32_t ip )
{
  optional<RouteEntry> result = nullopt;
  for ( auto entry : entries ) {
    uint8_t len = 32u - entry.prefix_length;
    // default route
    if ( entry.prefix_length > 32 )
      continue;
    if ( len == 32 && !result.has_value() ) {
      result = entry;
      continue;
    }

    if ( ( entry.route_prefix >> len ) == ( ip >> len )
         && ( !result.has_value() || entry.prefix_length > result->prefix_length ) )
      result = entry;
  }

  return result;
}

void Router::route()
{
  for ( auto& in : interfaces_ ) {
    auto optional_dgram = in.maybe_receive();
    if ( !optional_dgram.has_value() )
      continue;
    InternetDatagram& datagram = optional_dgram.value();

    if ( datagram.header.ttl <= 1 )
      continue;
    datagram.header.ttl--;
    datagram.header.compute_checksum();

    auto entry = findEntry( entries_, datagram.header.dst );
    if ( !entry.has_value() )
      continue;


    auto& sender_interface = interface( entry->interface_num );
    sender_interface.send_datagram( datagram,
                                    entry->next_hop.value_or( Address::from_ipv4_numeric( datagram.header.dst ) ) );
  }
}
