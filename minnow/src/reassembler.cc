#include "reassembler.hh"
#include <cstdint>

using namespace std;

Reassembler::Reassembler() : buffer(), buf(), end_index( -1 ), pending( 0 ) {}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  (void)first_index;
  (void)data;
  (void)is_last_substring;
  (void)output;

  if ( buf.empty() )
    buf.resize( output.capacity() );

  uint64_t bias_push = output.bytes_pushed();
  uint64_t insert_l = max( output.bytes_pushed(), first_index );
  uint64_t insert_r = min( first_index + data.size() - 1, output.available_capacity() + bias_push - 1 );

  if ( is_last_substring )
    end_index = first_index + data.size() - 1;

  if ( data.empty() && is_last_substring ) {
    output.close();
    return;
  }

  if ( insert_l - first_index >= data.size() )
    return;
  if ( insert_l > insert_r )
    return;

  for ( uint64_t i = insert_l; i <= insert_r; i++ ) {
    buf[i - bias_push] = data[i - first_index];
  }

  bool changed = true;
  while ( changed && !buffer.empty() ) {
    changed = false;
    auto upper = buffer.lower_bound( insert_l );

    // upper.first >= l, compare [l,r] with [uf, us]
    if ( upper != buffer.end() && insert_r + 1 >= upper->first ) {

      insert_r = max( upper->second, insert_r );
      pending -= upper->second - upper->first + 1;

      buffer.erase( upper );
      changed = true;
      continue;
    }

    if ( upper == buffer.begin() || buffer.empty() )
      break;
    auto lower = --upper;

    // lower.first < l, compare [lf, ls] with [l, r]
    if ( lower != buffer.end() && lower->second + 1 >= insert_l ) {

      insert_l =  lower->first;
      insert_r = max( lower->second, insert_r );
      pending -= lower->second - lower->first + 1;

      buffer.erase( lower );
      changed = true;
      continue;
    }
  }

  if ( insert_l == output.bytes_pushed() ) {
    uint64_t old_bias = output.bytes_pushed();
    output.push( buf.substr( insert_l - output.bytes_pushed(), insert_r - insert_l + 1 ) );
    bias_push = output.bytes_pushed();

    if ( insert_r == end_index ) {
      output.close();
    }

    for ( auto it = buffer.begin(); it != buffer.end(); it++ )
      for ( uint64_t i = it->first; i <= it->second; ++i )
        buf[i - bias_push] = buf[i - old_bias];
  } else {
    buffer[insert_l] = insert_r;
    pending += insert_r - insert_l + 1;
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return pending;
}
