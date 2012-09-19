#include <cassert>
#include <iostream>

#include "chatroom.h"
#include "simplechatserver.h"
#include "protocol.h"

using namespace std;

namespace SCS {

const char *NOTIFICATION_PREFIX = "=====> ";

Chatroom::Chatroom( const std::string &name )
	: m_Name(name), m_nNumberOfUsers(0)
{
}

Chatroom::~Chatroom( )
{
}

void Chatroom::addUser( int userSocket )
{
	pair<SocketCollection::iterator, bool> pr = m_UserSockets.insert( userSocket );

	if( pr.second ) // user was added successfully
	{
		notifyEveryoneThatUserJoined( userSocket );		
		m_nNumberOfUsers++;
	}
}

void Chatroom::removeUser( int userSocket )
{
	SocketCollection::iterator itr = m_UserSockets.find( userSocket );

	if( itr != m_UserSockets.end( ) ) // found user, so remove him
	{
		m_UserSockets.erase( itr );
		notifyEveryoneThatUserLeft( userSocket );

		m_nNumberOfUsers--;
	}
}

void Chatroom::notifyEveryone( const std::string &message, int type, int excludeUserSocket ) const
{
	SocketCollection::const_iterator itr;

	for( itr = m_UserSockets.begin( ); itr != m_UserSockets.end( ); ++itr )
	{
		if( *itr != excludeUserSocket )
		{
			NetMessaging::Protocol::sendServerMessage( *itr, type, message );
		}
	}
}

void Chatroom::sendMessage( int fromUserSocket, const std::string &message ) const
{
	SimpleChatServer *pServer = SimpleChatServer::getInstance( );
	User user( 0 );
	pServer->getUserFromSocket( fromUserSocket, user );

	std::string payload = user.username( ) + '\0' + m_Name + '\0' + message + '\0';

	#ifdef _DEBUG
	cout << "DEBUG Chatroom::sendMessage( ): payload = [" << payload << "] (Null bytes not shown)" << endl;
	#endif

	SocketCollection::const_iterator itr;
	for( itr = m_UserSockets.begin( ); itr != m_UserSockets.end( ); ++itr )
	{
		NetMessaging::Protocol::Message m;
		NetMessaging::Protocol::initializeMessage( m, NetMessaging::Protocol::MT_SEND_CHATROOM_MESSAGE, payload.length( ), &payload[ 0 ] );		
		NetMessaging::Protocol::sendMessage( *itr, m );
	}
}

void Chatroom::notifyEveryoneThatUserJoined( int userSocket ) const
{
	SimpleChatServer *pServer = SimpleChatServer::getInstance( );
	User user( 0 ); // gets filled in next line
	pServer->getUserFromSocket( userSocket, user );

	std::string message = m_Name + "\n" + user.username( ) + "@" + user.ipAddress( );
	notifyEveryone( message, NetMessaging::Protocol::MT_NOTIFY_USER_JOINED, userSocket );
}

void Chatroom::notifyEveryoneThatUserLeft( int userSocket ) const
{
	SimpleChatServer *pServer = SimpleChatServer::getInstance( );
	User user( 0 ); // gets filled in next line
	pServer->getUserFromSocket( userSocket, user );

	std::string message = m_Name + "\n" + user.username( ) + "@" + user.ipAddress( );
	notifyEveryone( message, NetMessaging::Protocol::MT_NOTIFY_USER_LEFT, userSocket );
}

} // end of namespace
