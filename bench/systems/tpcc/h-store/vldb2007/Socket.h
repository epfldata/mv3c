// Definition of the Socket class

#ifndef Socket_class
#define Socket_class
#ifndef SO_NOSIGPIPE
    #define SO_NOSIGPIPE MSG_NOSIGNAL
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <iostream>
#include <errno.h>

#include "Constants.h"

const int MAXHOSTNAME = 200;
const int MAXCONNECTIONS = 5;

class Socket
{
 public:
  Socket();
  virtual ~Socket();

  // Server initialization
  bool create();
  bool bind ( const int port );
  bool listen() const;
  bool accept ( Socket& ) const;

  // Client initialization
  bool connect ( const std::string host, const int port );

  // Data Transimission
  bool send ( char* toSend ) const;
  int recv ( char* s ) const;
  int recvN ( char *s );

  void set_non_blocking ( const bool );

  bool is_valid() const { return m_sock != -1; }

  // Wrapper to detect and print errors from socket return value.
  bool test_recv(int ret_value) {
    if (ret_value == -1) {
      std::cout << "errno == " << errno << "  in Socket::recv\n";
      return false;
    } else if (ret_value == 0) {
      return false;
    }
    return true;
  }

 private:

  int m_sock;
  sockaddr_in m_addr;


};

#endif
