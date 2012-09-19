#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <csignal>
using namespace std;
#include "simplechatserver.h"
#include "engine.h"
#include "protocol.h"

namespace SCS {

SimpleChatServer *SimpleChatServer::m_pInstance = NULL;

SimpleChatServer *SimpleChatServer::getInstance( )
{
    if( !m_pInstance )
	{
		m_pInstance = new SimpleChatServer( );
	}

    return m_pInstance;
}

SimpleChatServer::SimpleChatServer( )
: Server( ),
  m_nMaxChatrooms(0), 
  m_nMaxUsersPerChatroom(0),
  m_nNumberOfConnections(0),
  m_bVerbose(false)
{
}

SimpleChatServer::~SimpleChatServer( )
{
}


bool SimpleChatServer::initialize( unsigned int maxChatrooms, unsigned int maxUsersPerChatroom, unsigned short port, unsigned int maxConnectionsAllowed )
{
	if( !NetMessaging::initialize( ) ) return false;
    m_nMaxChatrooms               = maxChatrooms;

	if( !startListening( port, maxConnectionsAllowed ) )
	{
		return false;
	}

	Engine::onInfo( "Using address %s and port %u.", address( ), this->port( ) );
	Engine::onInfo( "Max Connections Allowed: %d", maxConnections( ) );	
    Engine::onInfo( "Max Chatrooms Allowed: %d", m_nMaxChatrooms );

    return true;
}

bool SimpleChatServer::deinitialize( )
{
	stopListening( );
	NetMessaging::deinitialize( );
    return true;
}

int SimpleChatServer::acceptConnection( )
{
	int clientSocket = Server::acceptConnection( );

    if( m_nNumberOfConnections >= maxConnections( ) )
    {		
		close( clientSocket );
		Engine::onInfo( "Max connection limit reached! Connection will be refused." );
		return -1;
    }

	generalLock.lock( );
    	m_nNumberOfConnections++;
	generalLock.unlock( );


    return clientSocket;
}

void SimpleChatServer::handleClient( int clientSocket )
{
    pthread_t threadID;
    ThreadArgs *args = new ThreadArgs;
    args->clientSocket = clientSocket;

    if( pthread_create( &threadID, NULL, SimpleChatServer::handleClient, args ) != 0 )
    {
		Engine::onError( "Failed to create thread to handle client." );
		return;
    }
}

void *SimpleChatServer::handleClient( void *thread_args )
{
    ThreadArgs *args = static_cast<ThreadArgs *>( thread_args ); 
    assert( args != NULL );
    SimpleChatServer *pServer = SimpleChatServer::getInstance( );
    bool bDone = false;
    NetMessaging::Protocol::Message message;

    while( !bDone )
    {
		//NetMessaging::Protocol::initializeMessage( message );

		if( NetMessaging::Protocol::receiveMessage( args->clientSocket, message ) == NetMessaging::Protocol::FAILED )
		{
			// remove user from all chatrooms in the case of an orderly shutdown...
			#ifdef _DEBUG
			Engine::onError( "Failed to receive message!" );
			#endif
			pServer->handleUserLeave( args->clientSocket, message );
			bDone = true;
		}
		else // received valid message & no errors occurred...
		{					
			/*
			 *	Here we handle the message that was received
			 * 	from the call to receiveMessage(). If handleMessage( )
			 * 	returns false, then we received a MT_USER_LEAVE or the
			 * 	user disconnected.
			 */
			#ifdef _DEBUG
			Engine::onInfo( "Handling received message..." );
			#endif

			if( !pServer->handleMessage( args->clientSocket, message ) )
			{
				// remove user from all chatrooms in the case of an orderly shutdown...
				pServer->handleUserLeave( args->clientSocket, message );
				bDone = true;
			}

			#ifdef _DEBUG
			Engine::onInfo( "Received message handling done..." );
			#endif

			NetMessaging::Protocol::freeMessageData( message ); //free data allocated in receiveMessage()
		}
    }

    pServer->handleDisconnect( args->clientSocket );

    // free memory and let the thread exit...
    delete args;
    pthread_exit( NULL );
    return NULL;
}

void SimpleChatServer::handleDisconnect( int clientSocket )
{
    // log the disconnection...
	generalLock.lock( );
		disconnectPeer( clientSocket );
		m_nNumberOfConnections--;
	generalLock.unlock( );
}

bool SimpleChatServer::handleMessage( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
    #ifdef _DEBUG
	std::string debugMsg = NetMessaging::Protocol::payloadString( msg.data, msg.header.dataSize );
    Engine::onInfo( "Client socket = %d, handling message %.4x with %s (size = %d).", clientSocket, msg.header.type, debugMsg.c_str( ), msg.header.dataSize );
    #endif

    switch( msg.header.type )
    {
		case NetMessaging::Protocol::MT_USER_ENTER:
			return handleUserEnter( clientSocket, msg );			
		case NetMessaging::Protocol::MT_USER_LEAVE:
			return handleUserLeave( clientSocket, msg );	// this case must return false		
		case NetMessaging::Protocol::MT_CHATROOM_LIST:
			return handleChatroomList( clientSocket, msg );
		case NetMessaging::Protocol::MT_USER_LIST:
			return handleUserList( clientSocket, msg );	
		case NetMessaging::Protocol::MT_ENTER_CHATROOM:	
			return handleEnterChatroom( clientSocket, msg );
		case NetMessaging::Protocol::MT_LEAVE_CHATROOM:
			return handleLeaveChatroom( clientSocket, msg );
		case NetMessaging::Protocol::MT_SEND_CHATROOM_MESSAGE:
			return handleSendChatroomMessage( clientSocket, msg );
			/*case MT_SEND_USER_MESSAGE:
			  return handleSendUserMessage( clientSocket, msg );*/
		default:
			Engine::onInfo( "Unknown message type %.8x received from client socket %d. We will ignore this.", msg.header.type, clientSocket );
			return true; // ignore this message.
    }
}

/*
 *  Message Handlers
 */
bool SimpleChatServer::handleUserEnter( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
    Engine::onInfo( "Client socket = %d, handleUserEnter( )", clientSocket );

    if( msg.data == NULL )
    {
		Engine::onInfo( "Client socket = %d, User supplied no username. The user will be disconnected. %p", clientSocket, msg.data );
		return false; // no username came along? so disconnect
    }

	std::string ip( Server::peerAddress( clientSocket ) );
	if( ip.length( ) == 0 ) // save ip address of remote user...
	{
		ip.assign( "Unknown IP" );
	}

	std::string username( msg.data );

    #ifdef _DEBUG
    cout << "DEBUG handleUserEnter( ): username = " << username << ", ip = " << ip << endl;
    #endif

    User user( clientSocket, username, ip );

	usersLock.lock( ); // crtical section...
		// check if username is already in use:
		// if so, disconnect
		// otherwise, proceed...
		UserCollection::iterator itr;
		for( itr = m_Users.begin( ); itr != m_Users.end( ); ++itr )
		{
			if( itr->username( ) == username )
			{
				usersLock.unlock( );
				return false; 
			}
		}

		// Insert the user 
    	pair<UserCollection::iterator, bool> pr = m_Users.insert( user );

		if( !pr.second ) // insertion failed
		{
			Engine::onInfo( "Client socket = %d, User tried to log in twice.", clientSocket );
		}

		// log some statistics
		logStats( );
    usersLock.unlock( );


    return true;
}

bool SimpleChatServer::handleUserLeave( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
    bool bReturn = false;
    Engine::onInfo( "Client socket = %d, handleUserLeave( )", clientSocket );
    User user( clientSocket );

	chatroomsLock.lock( );
		usersLock.lock( ); // crtical section...
			UserCollection::iterator itr = m_Users.find( user );

			if( itr != m_Users.end( ) ) // found user...
			{
				User::ChatroomCollection chatroomNameList = itr->chatrooms( );
				User::ChatroomCollection::iterator crNameItr;

				// remove the user from each chatroom he was participating in...
				for( crNameItr = chatroomNameList.begin( ); crNameItr != chatroomNameList.end( ); ++crNameItr )
				{
					TreeMapChatrooms::iterator crItr = m_Chatrooms.find( *crNameItr );

					if( crItr != m_Chatrooms.end( ) )
					{
						crItr->second.removeUser( clientSocket );			

						// obscure case: one user in chatroom disconnects, chatroom is removed.
						if( crItr->second.getNumberOfUsers( ) <= 0 ) // chatroom is empty so remove it
							m_Chatrooms.erase( crItr );
					}
				}

				#ifdef _DEBUG
				unsigned int nNumberOfUsers = m_Users.size( );
				#endif

				// remove the user from the main user list.
				m_Users.erase( itr ); // remove the user...				
				
				// log some statistics
				logStats( );

				#ifdef _DEBUG
				assert( m_Users.size( ) + 1 == nNumberOfUsers );
				#endif
			}
			else
			{
				bReturn = true; // avoid disconnecting client if MT_USER_LEAVE came before MT_USER_ENTER
			}
		usersLock.unlock( );
	chatroomsLock.unlock( );
    return bReturn; // return false on success
}

bool SimpleChatServer::handleChatroomList( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
    Engine::onInfo( "Client socket = %d, handleChatroomList( )", clientSocket );
	std::string chatroomList("");


	chatroomsLock.lock( ); // bof critical section...
    	TreeMapChatrooms::const_iterator itr;
    	for( itr = m_Chatrooms.begin( ); itr != m_Chatrooms.end( ); ++itr )
		{
			chatroomList += itr->first + '\n';
		}
    chatroomsLock.unlock( ); // eof critical section...

    #ifdef _DEBUG
    cout << "DEBUG handleChatroomList( ): Chatroom list = {";
    for( size_t i = 0; i < msg.header.dataSize; i++ )
    {
		if( msg.data[ i ] == '\n' )
		{ cout << ", ";	/*i++;*/ } // skip over extra '\n'
		else
		{ cout << msg.data[ i ]; }
    }
    cout << "}" << endl;
    //debugString( chatroomList );
    #endif

    if( m_Chatrooms.size( ) > 0 )
    {
		chatroomList.erase( chatroomList.length( ) - 1 ); // remove the extra '\n'
		chatroomList.append( 1, '\0' );
    }

    NetMessaging::Protocol::Message returnMsg;	
    NetMessaging::Protocol::initializeMessage( returnMsg, NetMessaging::Protocol::MT_CHATROOM_LIST, chatroomList.length( ), &chatroomList[ 0 ] ); 

    if( NetMessaging::Protocol::sendMessage( clientSocket, returnMsg ) == NetMessaging::Protocol::FAILED )
    {
		Engine::onError( "Client socket = %d, handleChatroomList( ) failed to send respone.", clientSocket );
		return false;
    }

    return true;
}

bool SimpleChatServer::handleUserList( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
    Engine::onInfo( "Client socket = %d, handleUserList( )", clientSocket );
	std::string userList("");
	std::string chatroomName( msg.data, msg.header.dataSize - 1 );


	chatroomsLock.lock( ); // bof critical section
		TreeMapChatrooms::const_iterator itrChatroom = m_Chatrooms.find( chatroomName );

		if( itrChatroom != m_Chatrooms.end( ) ) // found a chatroom
		{
			Chatroom::UserSocketCollection userSocketList = itrChatroom->second.getUsers( );
			Chatroom::UserSocketCollection::const_iterator itrUserSockets;

			for( itrUserSockets = userSocketList.begin( ); itrUserSockets != userSocketList.end( ); ++itrUserSockets )
			{
				User theUser( *itrUserSockets );

				usersLock.lock( ); // bof critical section
					UserCollection::const_iterator itrUsers = m_Users.find( theUser );

					if( itrUsers != m_Users.end( ) )
					{
						userList += itrUsers->username( ) + "@" + itrUsers->ipAddress( ) + '\n';
					}
				usersLock.unlock( ); // eof critical section
			}

			if( userSocketList.size( ) > 0 )
			{
				userList.erase( userList.length( ) - 1 ); // remove the extra '\n'
				userList.append( 1, '\0' );
			}
		}
	chatroomsLock.unlock( ); // eof critical section

    NetMessaging::Protocol::Message returnMsg;
    NetMessaging::Protocol::initializeMessage( returnMsg, NetMessaging::Protocol::MT_USER_LIST, userList.length( ), &userList[ 0 ] );

    if( NetMessaging::Protocol::sendMessage( clientSocket, returnMsg ) == NetMessaging::Protocol::FAILED )
    {
		Engine::onError( "Client socket = %d, handleUserList( ) failed to send respone.", clientSocket );
		return false;
    }

    return true;
}

bool SimpleChatServer::handleEnterChatroom( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
    Engine::onInfo( "Client socket = %d, handleEnterChatroom( )", clientSocket );
	std::string chatroomName( msg.data, msg.header.dataSize - 1 );

    #ifdef _DEBUG
    cout << "DEBUG handleEnterChatroom( ): chatroom name = " << chatroomName << endl;
    //debugString( chatroomName );
    #endif


	chatroomsLock.lock( ); // crtical section...
		TreeMapChatrooms::iterator itr = m_Chatrooms.find( chatroomName );

		if( itr != m_Chatrooms.end( ) ) // found existing chatroom
		{
			#ifdef _DEBUG
			cout << "DEBUG handleEnterChatroom( ): joining existing room."<< endl;
			#endif
			// this looks inefficient, but hash_set's iterator is actually
			// a const_iterator and therefore I cannot mutate the chatroom in
			// the hash_set
			User user( clientSocket );

			usersLock.lock( ); // bof critical section
				UserCollection::iterator userItr = m_Users.find( user );
				if( userItr != m_Users.end( ) )
				{
					User copy = *userItr;
					copy.addChatroom( chatroomName );
					m_Users.erase( userItr );
					m_Users.insert( copy );
				}
				else
				{ // error: could not find user...
					#ifdef _DEBUG
					SCS::Engine::onError( "Could not find user." );
					int count = 1;
					SCS::Engine::onInfo( "bof Chatroom List:" );
					for( UserCollection::const_iterator itr = m_Users.begin( ); itr != m_Users.end( ); ++itr )
						SCS::Engine::onInfo( "   %.2d %s (%d)", count++, itr->username( ).c_str( ), itr->socket( ) );
					SCS::Engine::onInfo( "eof Chatroom List:" );
					#endif
					usersLock.unlock( );
					chatroomsLock.unlock( );
					return false;				
				}		
			usersLock.unlock( ); // eof critical section

			itr->second.addUser( clientSocket ); // add client socket to chatroom
			
			// log some statistics
			logStats( );
		}
		else
		{ // chatroom not found so create one for the user
			#ifdef _DEBUG
			cout << "DEBUG handleEnterChatroom( ): creating room."<< endl;
			unsigned int sz = m_Chatrooms.size( );
			#endif

			User user( clientSocket );

			usersLock.lock( ); // bof critical section
				UserCollection::iterator userItr;
				userItr = m_Users.find( user );
				if( userItr != m_Users.end( ) )
				{
					User copy = *userItr;
					copy.addChatroom( chatroomName );
					m_Users.erase( userItr );
					m_Users.insert( copy );
				}
				else
				{ // error: could not find user...
					#ifdef _DEBUG
					SCS::Engine::onError( "Could not find user." );
					int count = 1;
					SCS::Engine::onInfo( "bof Chatroom List:" );
					for( UserCollection::const_iterator itr = m_Users.begin( ); itr != m_Users.end( ); ++itr )
						SCS::Engine::onInfo( "   %.2d %s (%d)", count++, itr->username( ).c_str( ), itr->socket( ) );
					SCS::Engine::onInfo( "eof Chatroom List:" );
					#endif
					usersLock.unlock( );
					chatroomsLock.unlock( );
					return false;				
				}		
			usersLock.unlock( ); // eof critical section

			Chatroom chatroom( chatroomName );
			chatroom.addUser( clientSocket );  // add client socket to new chatroom
			m_Chatrooms.insert( make_pair( chatroomName, chatroom ) );
			#ifdef _DEBUG
			assert( sz < m_Chatrooms.size( ) );
			#endif

			// log some statistics
			logStats( );
		}
    chatroomsLock.unlock( );

    return true;
}

bool SimpleChatServer::handleLeaveChatroom( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
    if( msg.data == NULL ) return false;
	std::string chatroomName( msg.data, msg.header.dataSize - 1 );

    #ifdef _DEBUG
    cout << "DEBUG handleLeaveChatroom( ): chatroom name = \"" << chatroomName << "\"" << endl;
    //debugString( chatroomName );


    TreeMapChatrooms::const_iterator crItr;
	std::string chatroomList;

    chatroomsLock.lock( ); // bof critical section...
    	for( crItr = m_Chatrooms.begin( ); crItr != m_Chatrooms.end( ); ++crItr )
		{
			chatroomList += "\"" + crItr->first + "\", ";
		}
    chatroomsLock.unlock( ); // eof critical section...

    cout << "Chatrooms = {" << chatroomList << "}" << endl;
    #endif

    chatroomsLock.lock( ); // crtical section...
		TreeMapChatrooms::iterator itr = m_Chatrooms.find( chatroomName );

		if( itr != m_Chatrooms.end( ) ) // found existing chatroom
		{
			// this looks inefficient, but hash_set's iterator is actually
			// a const_iterator and therefore I cannot mutate the chatroom in
			// the hash_set
			User user( clientSocket );			

			usersLock.lock( ); // bof critical section
				UserCollection::iterator userItr = m_Users.find( user );
				if( userItr != m_Users.end( ) )
				{					
					// Update user object; remove chatroom from user object.
					User copy = *userItr;
					copy.removeChatroom( chatroomName );
					m_Users.erase( userItr );
					m_Users.insert( copy );										
				}
			usersLock.unlock( ); // eof critical section

			// Remove user from chatroom.
			itr->second.removeUser( clientSocket );		

			if( itr->second.getNumberOfUsers( ) <= 0 ) // chatroom is empty so remove it
			{
				m_Chatrooms.erase( itr );
				
				// log some statistics
				logStats( );
			}
		}
		// else
		// this must be an error from the client, so
		// we will forgive him.
    chatroomsLock.unlock( );

    return true;
}

bool SimpleChatServer::handleSendChatroomMessage( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
	std::string chatroomName;
	std::string textMessage;
    const char *pData = static_cast<char *>( msg.data );
    size_t i = 0;

    // extract chatroom name
    for( ; pData[ i ] != '\0' && i < msg.header.dataSize; i++ )
	{
		chatroomName.append( 1, pData[ i ] );
	}

    i++;

    // extract text message
    for( ; pData[ i ] != '\0' && i < msg.header.dataSize; i++ )
	{
		textMessage.append( 1, pData[ i ] );
	}

    #ifdef _DEBUG
    cout << "DEBUG handleSendChatroomMessage( ): chatroom = " << chatroomName << ", textMessage = " << textMessage << endl;
    //debugString( chatroomName );
    //debugString( textMessage );
    #endif




    chatroomsLock.lock( ); // crtical section...
		TreeMapChatrooms::iterator itr = m_Chatrooms.find( chatroomName );

		if( itr != m_Chatrooms.end( ) ) // found existing chatroom
		{
			itr->second.sendMessage( clientSocket, textMessage );
		}
		#ifdef _DEBUG
		else
		{
			cout << "DEBUG handleSendChatroomMessage( ): Opps!" << endl;
			cout << "DEBUG Chatroom List Mappings = {";
			for( itr = m_Chatrooms.begin( ); itr != m_Chatrooms.end( ); ++itr )
			{
				cout << "(" << itr->first << ", " << itr->second.getName( ) << ")" << ", ";
			}
			cout << "\b\b}" << endl;
		}
		#endif
		// else
		// this must be an error from the client, so
		// we will forgive him.
    chatroomsLock.unlock( );
    return true;
}

bool SimpleChatServer::handleSendUserMessage( int clientSocket, const NetMessaging::Protocol::Message &msg )
{
    assert( false ); //feature not implemented yet.
    return true;
}

bool SimpleChatServer::getUserFromSocket( int clientSocket, User &user ) // not thread safe!
{
    User hashKeyUser( clientSocket );
    bool bRet = false;

    UserCollection::iterator itr = m_Users.find( hashKeyUser );

    if( itr != m_Users.end( ) ) // found user...
    {
		user = *itr;
		bRet = true;
    }

    return bRet;
}


bool SimpleChatServer::updateUser( User &user )
{
    bool bRet = false;

    usersLock.lock( ); // bof crtical section...
    	UserCollection::iterator itr = m_Users.find( user );
    	if( itr != m_Users.end( ) ) // found user...
		{
			m_Users.erase( itr );
			m_Users.insert( user );
			bRet = true;
		}
    usersLock.unlock( ); // eof crtical section...

    return bRet; // bRet = false;
}

bool SimpleChatServer::getCopyOfChatroom( const std::string &chatroomName, Chatroom &chatroom )
{
    bool bRet = false;

    chatroomsLock.lock( ); // bof crtical section...
		TreeMapChatrooms::iterator itr = m_Chatrooms.find( chatroomName );

		if( itr != m_Chatrooms.end( ) ) // found chatroom...
		{
			chatroom = itr->second;
			bRet = true;
		}
    chatroomsLock.unlock( ); // eof crtical section...

    return bRet; // bRet = false;
}

bool SimpleChatServer::updateChatroom( Chatroom &chatroom )
{
    bool bRet = false;

    chatroomsLock.lock( ); // bof crtical section...
		TreeMapChatrooms::iterator itr = m_Chatrooms.find( chatroom.getName( ) );

		if( itr != m_Chatrooms.end( ) ) // found chatroom...
		{
			itr->second = chatroom;
			bRet = true;
		}
    chatroomsLock.unlock( ); // eof crtical section...

    return bRet; // bRet = false;
}

void SimpleChatServer::logStats( )
{
	SCS::Engine::onInfo( "Statistics: # of Users: %d, # of Chatrooms: %d", m_Users.size( ), m_Chatrooms.size( ) );
}


} // end of namespace
