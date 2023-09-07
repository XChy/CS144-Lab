#include <cstddef>
#include <stdexcept>
#include <string_view>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity ), buffer(), closed( false ), hasError( false ), bytesPopped( 0 ), bytesPushed( 0 )
{}
void Writer::push( string data )
{

  int toPushLen = min( (int)data.size(), (int)available_capacity() );
  for ( int i = 0; i < toPushLen; ++i ) {
    buffer.push( data[i] );
  }
  bytesPushed += toPushLen;
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
  return capacity_ - buffer.size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytesPushed;
}

string_view Reader::peek() const
{
  return { &buffer.front(), 1 };
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
  for ( unsigned i = 0; i < len; ++i )
    buffer.pop();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer.size();
}

uint64_t Reader::bytes_popped() const
{
  return bytesPopped;
}
