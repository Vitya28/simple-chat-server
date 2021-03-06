///////////////////////////////////////////////////////////////////////////////////
//// Protocol.cc //////////////////////////////////////////////////////////////////
//// Wednesday, September 5, 2007 /////////////////////////////////////////////////
//// Copyright 2007 Joseph A. Marrero, http://www.manvscode.com/ //////////////////
//// joe@manvscode.com ////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
#include <string>
#include <cstring>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <cerrno>
#include "protocol.h"
#ifdef WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <netdb.h> //gethostbyname()
#endif

#ifdef _PROTOCOL_DEBUG
#include "engine.h"
#endif

namespace NetMessaging {


bool initialize( )
{
	#ifdef WIN32
	WORD version = MAKEWORD(2, 0);
	WSADATA wsadata;

	int error = WSAStartup(version, &wsadata );

	if( error != 0 ) return false;
	if( LOBYTE( wsadata.wVersion ) != 2 || HIBYTE( wsadata.wVersion ) != 0 )
	{
		deinitialize( );
		return false;
	}
	#endif

	return true;
}

void deinitialize( )
{

	#ifdef WIN32
	WSACleanup( );
	#endif
}




Server::Server( )
  : m_ServerSocket(-1)
{
	memset( &m_ServerAddress, 0, sizeof(struct sockaddr_in) );
}


bool Server::startListening( unsigned short _port, unsigned int _maxConnections )
{
	m_Port           = _port;
	m_MaxConnections = _maxConnections;

    // configure server address... 
    m_ServerAddress.sin_family      = AF_INET;
    m_ServerAddress.sin_port        = htons( m_Port );
    m_ServerAddress.sin_addr.s_addr = htonl( INADDR_ANY );

    // create a socket...
    if( (m_ServerSocket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP ) ) < 0 )
    {
		#ifdef _PROTOCOL_DEBUG
		SCS::Engine::onError( "Could not create server socket." );
		#endif
		return false;
    }

    if( bind( m_ServerSocket, (const struct sockaddr *) &m_ServerAddress, sizeof(struct sockaddr_in) ) < 0 )
    {
		close( m_ServerSocket );
		#ifdef _PROTOCOL_DEBUG
		SCS::Engine::onError( "Could not bind socket to address %s and m_Port %u.", inet_ntoa(m_ServerAddress.sin_addr), m_Port );
		#endif
		return false;
    }

	if( listen( m_ServerSocket, m_MaxConnections ) < 0 ) // instruct to listen on this socket...
	{
		close( m_ServerSocket );
		#ifdef _PROTOCOL_DEBUG
		SCS::Engine::onError( "Could not listen on socket." );
		#endif
		return false;
	}
	
	return true;
}

void Server::stopListening( )
{
	close( m_ServerSocket );
}

int Server::acceptConnection( )
{
	assert( m_ServerSocket >= 0 ); // need to call startListening() first!
	struct sockaddr_in clientAddress;
	socklen_t addressSize = sizeof( struct sockaddr_in );
	int clientSocket = accept( m_ServerSocket, (struct sockaddr *) &clientAddress, &addressSize );

    /*
   struct timeval timeOut;
   memset( &timeOut, 0, sizeof(struct timeval) );
   timeOut.tv_sec = 30; // twenty seconds...
   timeOut.tv_usec = 0;
   setsockopt( clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeOut, sizeof(struct timeval) );
   setsockopt( clientSocket, SOL_SOCKET, SO_SNDTIMEO, &timeOut, sizeof(struct timeval) );
   */

	if( clientSocket < 0 )
	{
		#ifdef _PROTOCOL_DEBUG
		SCS::Engine::onError( "Could not accept connection." );
		#endif
	}

	#ifdef _PROTOCOL_DEBUG
    SCS::Engine::onInfo( "Connection established with client %s (client socket = %d).", inet_ntoa( clientAddress.sin_addr ), clientSocket );
	#endif

	return clientSocket;
}

void Server::disconnectPeer( int peerSocket )
{
	assert( peerSocket >= 0 );
	struct sockaddr_in clientAddress;
	socklen_t addressSize = sizeof( struct sockaddr_in );
	getpeername( peerSocket, (struct sockaddr *) &clientAddress, &addressSize );

	#ifdef _PROTOCOL_DEBUG
	SCS::Engine::onInfo( "Closing connection with %s.", inet_ntoa( clientAddress.sin_addr ) );
	#endif

	close( peerSocket );	// close the connection		
}

const char *Server::peerAddress( int peerSocket )
{
    struct sockaddr_in clientAddress;
    socklen_t clientAddressSize = sizeof( struct sockaddr_in );

    // save ip address of remote user...
    if( getpeername( peerSocket, (struct sockaddr *) &clientAddress, &clientAddressSize ) == 0 ) // on success
	{
		return inet_ntoa( clientAddress.sin_addr );
	}

	return NULL;
}



///////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Protocol Stuff /////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
void Protocol::initializeMessage( Message &msg, MessageType type /*= 0*/, size_t dataSize /*= 0*/, const char *pData /*= NULL*/  )
{
    msg.header.marker = PROTOCOL_MARKER;
    msg.header.type = type;
    msg.header.dataSize = dataSize;
    msg.data = const_cast<char *>( pData );
}

bool Protocol::sendServerMessage( int clientSocket, MessageType type, const std::string &message )
{
    Message msg;
    initializeMessage( msg, type, message.length( ) + 1 /* plus 1 for '\0'*/, const_cast<char *>( &message[ 0 ] ) );

    if( !sendMessage( clientSocket, msg ) )
		return false;

    return true;
}

bool Protocol::sendErrorMessage( int clientSocket, const std::string &error )
{
    Message errMsg;
    initializeMessage( errMsg, MT_NOTIFY_ERROR, error.length( ) + 1 /* plus 1 for '\0'*/, const_cast<char *>( &error[ 0 ] ) );

    if( !sendMessage( clientSocket, errMsg ) )
		return false;

    return true;
}

Protocol::Result Protocol::receiveMessage( int clientSocket, Message &msg )
{
    if( _receiveMessage( clientSocket, msg ) == false ) // on failure, handle it...
    {
		#ifdef _PROTOCOL_DEBUG
		SCS::Engine::onError( "receivedMessage(): errno = %d", errno );
		#endif

		#ifdef _PROTOCOL_SIMULATE_LATENCY
		sleep( 1 );
		#endif

		switch( errno )
		{
			case EINTR: // interrupted by signal...
			case EAGAIN: // EWOULDBLOCK
				return TRYAGAIN;
			case ENOTSOCK: // client socket is not a socket.
			case ENOTCONN: // not connected yet.
			case EINVAL: // invalid argument passed.
			case EFAULT: // the receive buffer is outside process's address space
			case EBADF:  // bad socket/file descriptor
				assert( false ); // these are programming errors
			case ECONNREFUSED:
			default:
				return FAILED;
		}
    }

	#ifdef _PROTOCOL_SIMULATE_LATENCY
	sleep( 1 );
	#endif

    return SUCCESS;
}

Protocol::Result Protocol::sendMessage( int clientSocket, const Message &msg )
{
    // the const_cast here is needed because the data has to be encoded to
    // network Byte order (but the data is not changed).
    if( _sendMessage( clientSocket, const_cast<Message &>(msg) ) == false ) // on failure, handle it...
    {
		#ifdef _PROTOCOL_SIMULATE_LATENCY
		sleep( 1 );
		#endif

		switch( errno )
		{
			case EINTR: // interrupted by signal...
			case EAGAIN: // EWOULDBLOCK
				return TRYAGAIN;
			case ENOTSOCK: // client socket is not a socket.
			case ENOTCONN: // not connected yet.
			case EINVAL: // invalid argument passed.
			case EFAULT: // an argument is outside process's address space
			case EBADF:  // bad socket/file descriptor
			case EOPNOTSUPP: // some socket flag is inappropriate for this socket type.
				assert( false ); // these are programming errors
			case ECONNRESET: // connection reset by peer
			case ENOMEM: // no memory available
			default:
				return FAILED;
		}
    }


	#ifdef _PROTOCOL_SIMULATE_LATENCY
	sleep( 1 );
	#endif

    return SUCCESS;
}

/*
 * 	Free allocated memory from receiveMessage( )
 */
void Protocol::freeMessageData( Message &m )
{
    if( m.header.dataSize > 0 )
    {
		#ifdef _PROTOCOL_DEBUG	
		const std::string &dbg = payloadString( m.data, m.header.dataSize );
		SCS::Engine::onInfo( "Freeing message; type = %.4x, payload = %s (size = %d).", m.header.type, dbg.c_str( ), m.header.dataSize );
		#endif
		delete [] m.data;
    }
}

std::string Protocol::payloadString( const char *data, size_t size )
{
    std::string payloadCopy(size + 2, '[' );
    std::replace_copy( data, data + size, payloadCopy.begin( ) + 1, '\0', '&' ); // Replace NULL bytes for string printing.
    payloadCopy[ size + 1 ] = ']';
    return payloadCopy;		
}


bool Protocol::_receiveMessage( int clientSocket, Message &msg )
{
    unsigned int rBytes = 0;
    int rv = 0;
    char *pHeader = (char *) &msg.header;

    // receive the message header...
    for( rBytes = 0, rv = 0; rBytes < sizeof(MessageHeader); rBytes += rv )
    {
		if( (rv = recv( clientSocket, pHeader + rBytes, sizeof(MessageHeader) - rBytes, 0 ) ) < 0 )
		{				
			//if( errno == EINTR || errno == EAGAIN ) continue;
			
			#ifdef _PROTOCOL_DEBUG
			SCS::Engine::onError( "Receive message header error." );
			#endif

			return false;
		}
		else if( rv == 0 )
		{
			return false; // connection closed by peer
		}
    }

    // convert to message header to host order...
	assert( sizeof(msg.header.marker) == sizeof(short) ); // ensure use of ntohs()
    msg.header.marker = ntohs( msg.header.marker );
	assert( sizeof(msg.header.type) == sizeof(short) ); // ensure use of ntohs()
    msg.header.type = ntohs( msg.header.type );
    msg.header.dataSize = ntohl( msg.header.dataSize );

    if( !isMessage( msg ) )
    {
		#ifdef _PROTOCOL_DEBUG
		SCS::Engine::onError( "Marker mismatch error on received message; message ignored and connection with peer will be dropped. " );
		#endif
		return false;
    }

    if( msg.header.dataSize > 0 )
    {
		// allocate memory for data...
		msg.data = new char[ msg.header.dataSize ];

		// receive the data...
		for( rBytes = 0, rv = 0; rBytes < (unsigned) msg.header.dataSize; rBytes += rv )
		{
			if( (rv = recv( clientSocket, msg.data + rBytes, msg.header.dataSize - rBytes, 0 ) ) < 0 )
			{
				//if( errno == EINTR || errno == EAGAIN ) continue;
				
				#ifdef _PROTOCOL_DEBUG
				SCS::Engine::onError( "Received message data error." );
				#endif

				delete [] msg.data;
				return false;
			}
			else if( rv == 0 )
			{
				delete [] msg.data;
				return false; // connection closed by peer
			}
		}
    }

    #ifdef _PROTOCOL_DEBUG
    const std::string &payloadCopy = payloadString( msg.data, msg.header.dataSize );
    SCS::Engine::onInfo( "Received MSG %.4d %s (size = %d)", msg.header.type, payloadCopy.c_str( ), msg.header.dataSize );
    #endif
    return true;
}

bool Protocol::_sendMessage( int clientSocket, Message &msg )
{
    assert( isMessage( msg ) == true );
    assert( msg.header.type != 0 );
    assert( msg.header.dataSize >= 0 );
    int dataSize = msg.header.dataSize;

    #ifdef _PROTOCOL_DEBUG
    const std::string &payloadCopy = payloadString( msg.data, msg.header.dataSize );
    SCS::Engine::onInfo( "Sending MSG %.4d %s (size = %d)", msg.header.type, payloadCopy.c_str( ), msg.header.dataSize );
    #endif

    // convert to message header to network order...
	assert( sizeof(msg.header.marker) == sizeof(short) ); // ensure use of htons()
    msg.header.marker = htons( PROTOCOL_MARKER );
	assert( sizeof(msg.header.type) == sizeof(short) ); // ensure use of htons()
    msg.header.type = htons( msg.header.type );
    msg.header.dataSize = htonl( msg.header.dataSize );

    int count = 0;
    int size = sizeof( MessageHeader );

    // send the header...
    while( size > 0 )
    {
		int sentBytes = send( clientSocket, reinterpret_cast<char *>(&msg.header + count), size, 0 );

		if( sentBytes <= 0 )
		{
			//if( errno == EINTR || errno == EAGAIN ) continue;
			
			#ifdef _PROTOCOL_DEBUG
			SCS::Engine::onError( "Sending message header error." );
			#endif

			return false;
		}

		size -= sentBytes;
		count += sentBytes;
    }

    count = 0;

    while( dataSize > 0 )
    {
		// send the data...
		int sentBytes = send( clientSocket, msg.data + count, dataSize, 0 );

		if( sentBytes <= 0 )
		{
			//if( errno == EINTR || errno == EAGAIN ) continue;

			#ifdef _PROTOCOL_DEBUG
			SCS::Engine::onError( "Sending message data error." );
			#endif

			return false;
		}

		dataSize -= sentBytes;
		count += sentBytes;
    }

    return true;
}

// TO DO REWRITE THIS! gethostbyname() is obsolete.
bool Protocol::resolveName( const char *pName, unsigned long *address )
{
	struct hostent *host;
		
	if( (host = gethostbyname( pName ) ) == NULL )
		return false;
	
	*address = *( (unsigned long *) host->h_addr_list[ 0 ] );
	return true;	
}

///////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Byte-order Stuff ///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
bool Protocol::isBigEndian( )
{
    ShortAndBytes check;
    check.s = 1;
	
    if( check.bytes[ 0 ] == 1 )
	{
		return false; //little endian
	}

    return true;
}

void Protocol::swapEvery2Bytes( Byte *&mem, size_t size )
{
    // If we have an odd number of bytes, then 
    // we subtract 1 and swap up until that size.
    size -= (size % 2);

    for( size_t i = 0; i < size - 1; i++ )
		std::swap( mem[ i ], mem[ i + 1 ] );
}
///////////////////////////////////////////////////////////////////////////////////
//////////////////////////// End of Byte-order Stuff //////////////////////////////
///////////////////////////////////////////////////////////////////////////////////
}// end of namespace
