#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_
///////////////////////////////////////////////////////////////////////////////////
//// Protocol.h ///////////////////////////////////////////////////////////////////
//// Wednesday, September 5, 2007 /////////////////////////////////////////////////
//// Copyright 2007 Joseph A. Marrero, http://www.manvscode.com/ //////////////////
//// joe@manvscode.com ////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
#include <cassert>
#include <string>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

namespace NetMessaging {

bool initialize( );
void deinitialize( );

#ifdef WIN32
typedef int socklen_t;
#define ECONNREFUSED WSAECONNREFUSED
#define close closesocket
#endif

class Server
{
  protected:
	int m_ServerSocket;
    struct sockaddr_in m_ServerAddress;
	unsigned short m_Port;
	unsigned int m_MaxConnections;

  public:
	static const short DEFAULT_PORT          = 7575;
	static const int DEFAULT_MAX_CONNECTIONS = 50;

	Server( );

	bool startListening( unsigned short _port = DEFAULT_PORT, unsigned int _maxConnections = DEFAULT_MAX_CONNECTIONS );
	void stopListening( );
	int acceptConnection( );

	void disconnectPeer( int peerSocket );

	static const char *peerAddress( int peerSocket );

	int serverSocket( ) const;
	unsigned short port( ) const;
	const char *address( ) const;
	unsigned int maxConnections( ) const;

};

inline int Server::serverSocket( ) const
{ 
	assert( m_ServerSocket >= 0 ); 
	return m_ServerSocket;
}

inline unsigned short Server::port( ) const
{ 
	assert( m_ServerSocket >= 0 ); 
	return m_Port;
}

inline const char *Server::address( ) const
{ 
	assert( m_ServerSocket >= 0 ); 
	return inet_ntoa( m_ServerAddress.sin_addr );
}

inline unsigned int Server::maxConnections( ) const
{ 
	assert( m_ServerSocket >= 0 ); 
	return m_MaxConnections;
}


class Protocol
{
  public:
	enum Result {
		FAILED = 0,
		TRYAGAIN,
		SUCCESS 
	};



	typedef short         Marker;
	typedef short         MessageType;
    typedef unsigned char Byte;


    static const Marker PROTOCOL_MARKER = 0xFFEF;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////// PROTOCOL DEFINITION /////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///////////////////////// This section defines message types and their associated parameters. /////////////////////////////
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//                 Message                        Message       Sent Parameters (params. seperated by "\n")	         //
	//                  Type                           Code         [ args (from) ]  where 'from' is either server or client //	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static const MessageType MT_NO_MESSAGE                 = 0x00000000;
    static const MessageType MT_USER_ENTER                 = 0x00000001;  // user name (client)
    static const MessageType MT_USER_LEAVE                 = 0x00000002;  // NIL (client)
    static const MessageType MT_CHATROOM_LIST              = 0x00000003;  // NIL (client), list of chatrooms (server)
    static const MessageType MT_USER_LIST                  = 0x00000004;  // chatroom name (client), list of username@ip (server)
    static const MessageType MT_ENTER_CHATROOM             = 0x00000005;  // chatroom name (client)
    static const MessageType MT_LEAVE_CHATROOM             = 0x00000006;  // chatroom name (client)
    static const MessageType MT_SEND_CHATROOM_MESSAGE      = 0x00000007;  // chatroom name and message (client), username, chatroom and message (server)
    static const MessageType MT_SERVER_CHATROOM_MESSAGE    = 0x00000008;  // message (server)
    static const MessageType MT_SEND_USER_MESSAGE          = 0x00000009;  // username and message (client), other user name and message (server)
    static const MessageType MT_NOTIFY_ERROR               = 0x0000000A;  // error message (server)
    static const MessageType MT_NOTIFY_USER_JOINED         = 0x0000000B;  // Chatroom Username IP (server)
    static const MessageType MT_NOTIFY_USER_LEFT           = 0x0000000C;  // Chatroom Username@IP (server)


    /*
     * 	Message Definition
     *
     *	This section defines the structure of
     *	protocol messages.
     */
    #pragma pack(push, 1) // align to 2 bytes
    typedef struct tagMessageHeader {
		Marker marker; // = 0xFFEF;
		MessageType type;
		size_t dataSize;
    } MessageHeader;

    typedef struct tagMessage {
		MessageHeader header;
		char *data; // avoids endian problems
    } Message;
    #pragma pack(pop)

    static void initializeMessage( Message &msg, MessageType type = 0, size_t dataSize = 0, const char *pData = NULL );
    static bool isMessage( const Message &msg );
    static bool sendServerMessage( int clientSocket, MessageType type, const std::string &message );
    static bool sendErrorMessage( int clientSocket, const std::string &error );
    static void freeMessageData( Message &m );
    static Result receiveMessage( int clientSocket, Message &msg );
    static Result sendMessage( int clientSocket, const Message &msg );  
    static std::string payloadString( const char *data, size_t size );
	static bool resolveName( const char *pName, unsigned long *address );

    static bool isBigEndian( );
    static void swap2Bytes( Byte *&mem );
    static void swapEvery2Bytes( Byte *&mem, size_t size );

  ///////////////////////////////////////////////////////////////////////
  //////////////////// PRIVATE IMPLEMENTATION DETAILS ///////////////////
  ///////////////////////////////////////////////////////////////////////
  private:
    static bool _receiveMessage( int clientSocket, Message &msg ); // no error handling...
    static bool _sendMessage( int clientSocket, Message &msg ); // no error handling...

    union ShortAndBytes {
		unsigned short s;
		Byte bytes[2];
    };

    static void hton( void *mem, size_t size );
    static void ntoh( void *mem, size_t size );
};


inline bool Protocol::isMessage( const Message &msg )
{ return msg.header.marker == PROTOCOL_MARKER; }

#ifdef BIG_ENDIAN
inline void Protocol::hton( void *mem, size_t size ){} // Already in network-order; no implementation by intention.
inline void Protocol::ntoh( void *mem, size_t size ){} // Already in network-order; no implementation by intention.
#elif LITTLE_ENDIAN
inline void Protocol::hton( void *mem, size_t size )
{ swapEvery2Bytes( static_cast<Byte *>(mem), size ); }

inline void Protocol::ntoh( void *mem, size_t size )
{ swapEvery2Bytes( static_cast<Byte *>(mem), size ); }
#else // check for endian-ness at runtime...
inline void Protocol::hton( void *mem, size_t size )
{
	bool bBigEndian = isBigEndian( );
	if( bBigEndian ) return;

	Byte *theBytes = static_cast<Byte *>( mem );
	swapEvery2Bytes( theBytes, size );
}

inline void Protocol::ntoh( void *mem, size_t size )
{
	bool bBigEndian = isBigEndian( );
	if( bBigEndian ) return;

	Byte *theBytes = static_cast<Byte *>( mem );
	swapEvery2Bytes( theBytes, size );
}
#endif

}// end of namespace
#endif
