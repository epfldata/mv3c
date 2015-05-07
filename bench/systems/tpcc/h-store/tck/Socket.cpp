// Implementation of the Socket class.


#include "Socket.h"
#include "string.h"
#include <string.h>
#include <fcntl.h>

#include <assert.h>

#define min(x,y) ((x<y)?x:y)

Socket::Socket() :
  m_sock ( -1 )
{

  memset ( &m_addr,
	   0,
	   sizeof ( m_addr ) );

}

Socket::~Socket()
{
  if ( is_valid() )
    ::close ( m_sock );
}

bool Socket::create()
{
  m_sock = socket ( AF_INET,
		    SOCK_STREAM,
		    0 );

  if ( ! is_valid() )
    return false;


  // TIME_WAIT - argh
  int on = 1;
  if ( setsockopt ( m_sock, SOL_SOCKET, SO_REUSEADDR, ( const char* ) &on, sizeof ( on ) ) == -1 )
    return false;


  return true;

}



bool Socket::bind ( const int port )
{

  if ( ! is_valid() )
    {
      return false;
    }



  m_addr.sin_family = AF_INET;
  m_addr.sin_addr.s_addr = INADDR_ANY;
  m_addr.sin_port = htons ( port );

  int bind_return = ::bind ( m_sock,
			     ( struct sockaddr * ) &m_addr,
			     sizeof ( m_addr ) );


  if ( bind_return == -1 )
    {
      return false;
    }

  return true;
}


bool Socket::listen() const
{
  if ( ! is_valid() )
    {
      return false;
    }

  int listen_return = ::listen ( m_sock, MAXCONNECTIONS );


  if ( listen_return == -1 )
    {
      return false;
    }

  return true;
}


bool Socket::accept ( Socket& new_socket ) const
{
  int addr_length = sizeof ( m_addr );
  new_socket.m_sock = ::accept ( m_sock, ( sockaddr * ) &m_addr, ( socklen_t * ) &addr_length );

  if ( new_socket.m_sock <= 0 )
    return false;
  else
    return true;
}


bool Socket::send ( char* toSend ) const
{
  int status = ::send ( m_sock, toSend, *(int*)toSend + sizeof(int), SO_NOSIGPIPE );
  //printf("Socket: Sent %d bytes\n", (*(int*)toSend + sizeof(int)));
  if ( status == -1 )
    {
      return false;
    }
  else
    {
      return true;
    }
}

// Receives the next logical message.
// Logical messages may be arbitrarily chunked
// across physical messages: pain!
int Socket::recv(char* s) const {
  static char buf [MAXRECV + 1];
  static int pos_in_buffer = 0;
  static int num_remaining = 0;

  if (pos_in_buffer == 0) {
    memset (buf, 0, MAXRECV + 1);
    num_remaining = ::recv (m_sock, buf, MAXRECV, 0);
    if (num_remaining == -1) {
      std::cout << "errno == " << errno << "  in Socket::recv\n";
      return 0;
    } else if (num_remaining == 0) {
      return 0;
    }
  } else if (num_remaining < sizeof(int)) {
    // Spill over (1, 2, or 3 chars): annoyance!!!
    // Required to determine correct length of next message.
    char temp[sizeof(int)-1];
    for (int i = 0; i < num_remaining; ++i) {
      temp[i] = buf[pos_in_buffer+i];
    }
    for (int i = 0; i < num_remaining; ++i) {
      buf[i] = temp[i];
    }
    memset(buf+num_remaining, 0, MAXRECV + 1 - num_remaining);
    int len = ::recv(m_sock, buf+num_remaining, MAXRECV - num_remaining, 0);
    if (len == -1) {
      std::cout << "errno == " << errno << "  in Socket::recv\n";
      return 0;
    } else if (len == 0) {
      return 0;
    }
    num_remaining += len;
    assert (num_remaining >= sizeof(int));
    pos_in_buffer = 0;
  }

  int next_msg_len = ((int*)(buf + pos_in_buffer))[0] + sizeof(int);
  //printf("Socket: pos_in_buffer = %d num_remaining (in buffer) = %d next_msg_len = %d\n",
  //        pos_in_buffer, num_remaining, next_msg_len);

  int pos_in_next_msg = 0;
  
  do {
    int num_to_copy = min(num_remaining, next_msg_len-pos_in_next_msg);
    memcpy(s+pos_in_next_msg, buf + pos_in_buffer, num_to_copy);
    pos_in_next_msg += num_to_copy;
    pos_in_buffer += num_to_copy;
    num_remaining -= num_to_copy;
    if (pos_in_next_msg < next_msg_len) {
      num_remaining = ::recv(m_sock, buf, MAXRECV, 0);
      assert (num_remaining > 0);
      pos_in_buffer = 0;
    } else if (num_remaining == 0) {
      pos_in_buffer = 0;
    }
  } while (pos_in_next_msg < next_msg_len);
  //printf("End of Socket recv: pos_in_buffer = %d num_remaining (in buffer) = %d\n",
  //        pos_in_buffer, num_remaining);

  return next_msg_len;
}

// Efficient Mini-Session layer for TPC-C Server.
// Receive multiple logical messages into the given buffer.
// If less than MAXRECV message size available, return 
// whatever is available.
// Return the number of messages received, or 0 if error.
int Socket::recvN(char *s) {
  // Spillover buffer from previous call().
  static char spillover_buf[SOCK_BATCH_SIZE+1];

  // Number of bytes remaining in spillover buffer.
  static int num_remaining = 0;

  //assert (num_remaining <= SOCK_BATCH_SIZE);
  //printf("Begin recvN: num_remaining = %d\n", num_remaining);
  if (num_remaining > 0) {
    memmove(s, spillover_buf, num_remaining);
  }

  int len = ::recv(m_sock, s + num_remaining, SOCK_BATCH_SIZE - num_remaining, 0);
  //printf("recvN: recv(%d) returned %d\n", SOCK_BATCH_SIZE - num_remaining, len);
  if (!test_recv(len)) return 0;
  int s_size = num_remaining + len;
  //assert (s_size >= sizeof(int));
  num_remaining = 0;

  //printf("recvN: num_remaining = 0 s_size = %d\n", s_size);
  // Figure out actual number of messages in this.
  // Copy rest to spillover buffer.
  int s_offset = 0;
  int N = 0; // Message count.

  while (s_offset + sizeof(int) <= s_size) {
    int msg_len = ((int*)(s + s_offset))[0] + sizeof(int);
    if (s_offset + msg_len <= s_size) {
      //printf("recvN: s_offset = %d msg_len = %d s_size = %d N=%d\n",
      //        s_offset, msg_len, s_size, N);
      s_offset += msg_len;
      N++;
      continue;
    } else break;
  }

  // Copy rest of bytes into spillover buf and return.
  num_remaining = s_size - s_offset;
  //printf("Copying num_remaining %d = %d (s_size) - %d (s_offset) to spillover buf\n",
  //        num_remaining, s_size, s_offset);
  //assert (num_remaining <= SOCK_BATCH_SIZE);
  memmove(spillover_buf, s + s_offset, num_remaining);
  return N;
}

bool Socket::connect ( const std::string host, const int port )
{
  if ( ! is_valid() ) return false;

  m_addr.sin_family = AF_INET;
  m_addr.sin_port = htons ( port );

  int status = inet_pton ( AF_INET, host.c_str(), &m_addr.sin_addr );

  if ( errno == EAFNOSUPPORT ) return false;

  status = ::connect ( m_sock, ( sockaddr * ) &m_addr, sizeof ( m_addr ) );

  if ( status == 0 )
    return true;
  else
    return false;
}

void Socket::set_non_blocking ( const bool b )
{

  int opts;

  opts = fcntl ( m_sock,
		 F_GETFL );

  if ( opts < 0 )
    {
      return;
    }

  if ( b )
    opts = ( opts | O_NONBLOCK );
  else
    opts = ( opts & ~O_NONBLOCK );

  fcntl ( m_sock,
	  F_SETFL,opts );

}
