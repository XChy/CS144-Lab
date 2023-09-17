#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t cycle = 1ll << 32;
  uint64_t n_cycle = checkpoint / cycle;
  uint64_t diff = raw_value_ - zero_point.raw_value_;
  uint64_t upper = ( n_cycle + 1ll ) * cycle + diff;
  uint64_t middle = n_cycle * cycle + diff;
  uint64_t lower = ( n_cycle - 1ll ) * cycle + diff;
  if ( ( ( n_cycle == 0 && cycle <= diff ) || n_cycle != 0 ) && checkpoint <= ( lower + middle ) / 2 )
    return lower;
  if ( checkpoint <= ( middle + upper ) / 2 )
    return middle;
  else
    return upper;
}
