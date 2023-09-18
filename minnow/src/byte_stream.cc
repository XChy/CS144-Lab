#include <cstddef>
#include <stdexcept>
#include <string_view>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity )
  , buffer()
  , buffer_actual()
  , closed( false )
  , hasError( false )
  , bytesPopped( 0 )
  , bytesPushed( 0 )
  , bytesBuffered( 0 )
{}

uint64_t ByteStream::capacity() const
{
  return capacity_;
}

void Writer::push( string data )
{
  if ( data.empty() )
    return;
  int toPushLen = min( (int)data.size(), (int)available_capacity() );
  buffer_actual.push( std::move( data ) );
  buffer.push( string_view( buffer_actual.back() ).substr( 0, toPushLen ) );
  bytesPushed += toPushLen;
  bytesBuffered += toPushLen;
}

void Writer::close()
{
  closed = true;
}

void Writer::set_error()
{
  hasError = true;
}

bool Writer::is_closed() const
{
  return closed;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - bytesBuffered;
}

uint64_t Writer::bytes_pushed() const
{
  return bytesPushed;
}

string_view Reader::peek() const
{
  if ( !buffer.empty() )
    return buffer.front();
  else
    return string_view();
}

bool Reader::is_finished() const
{
  return closed && buffer.empty();
}

bool Reader::has_error() const
{
  return hasError;
}

void Reader::pop( uint64_t len )
{
  bytesPopped += len;
  for ( unsigned i = 0; i < len; ) {
    if ( buffer.front().size() > len - i ) {
      buffer.front() = buffer.front().substr( len - i );
      i = len;
    } else {
      i += buffer.front().size();
      buffer.pop();
      buffer_actual.pop();
    }
  }

  while ( !buffer.empty() && buffer.front().empty() )
    buffer.pop();
  bytesBuffered -= len;
}

uint64_t Reader::bytes_buffered() const
{
  return bytesBuffered;
}

uint64_t Reader::bytes_popped() const
{
  return bytesPopped;
}
