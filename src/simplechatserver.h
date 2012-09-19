#ifndef _SIMPLECHATSERVER_H_
#define _SIMPLECHATSERVER_H_

#include <map>
#include "synchronize.h"
#include "protocol.h"
#include "chatroom.h"
#include "user.h"

namespace SCS {

class SimpleChatServer : private NetMessaging::Server
{
  public:
    static const unsigned int DEFAULT_PORT = 7575;

    typedef struct tagThreadArgs {
		int clientSocket;
    } ThreadArgs;		

    typedef std::map<std::string, Chatroom> TreeMapChatrooms;
    typedef std::set<User> UserCollection;
	
  public:
    static SimpleChatServer *getInstance( );
    ~SimpleChatServer( );
	
    bool initialize( unsigned int maxChatrooms, unsigned int maxUsersPerChatroom, unsigned short port, unsigned int maxConnectionsAllowed );
    bool deinitialize( );
    int acceptConnection( );
  
    void handleClient( int clientSocket );
    static void *handleClient( void *thread_args );

    bool handleMessage( int clientSocket, const NetMessaging::Protocol::Message &msg );

    bool getUserFromSocket( int clientSocket, User &user );
    bool updateUser( User &user );
    bool getCopyOfChatroom( const std::string &chatroomName, Chatroom &chatroom );
    bool updateChatroom( Chatroom &chatroom );


	void logStats( );
  
  private:
    SimpleChatServer( );

    /*
     *  Message Handlers
     */
    bool handleUserEnter( int clientSocket, const NetMessaging::Protocol::Message &msg );
    bool handleUserLeave( int clientSocket, const NetMessaging::Protocol::Message &msg );
    bool handleChatroomList( int clientSocket, const NetMessaging::Protocol::Message &msg );
    bool handleUserList( int clientSocket, const NetMessaging::Protocol::Message &msg );
    bool handleEnterChatroom( int clientSocket, const NetMessaging::Protocol::Message &msg );
    bool handleLeaveChatroom( int clientSocket, const NetMessaging::Protocol::Message &msg );
    bool handleSendChatroomMessage( int clientSocket, const NetMessaging::Protocol::Message &msg );
    bool handleSendUserMessage( int clientSocket, const NetMessaging::Protocol::Message &msg );
    void handleDisconnect( int clientSocket );

  private:
    static SimpleChatServer *m_pInstance;
    TreeMapChatrooms         m_Chatrooms;
    UserCollection           m_Users;
  
    /*
     * 	Be careful; the chatroom mutex should always be locked first, followed
     * 	by the user's mutex. This should avoid most deadlock scenarios.
     */
    Lock            chatroomsLock;
    Lock            usersLock;
    Lock            generalLock;
    unsigned int    m_nMaxChatrooms;
    unsigned int    m_nMaxUsersPerChatroom;
    unsigned int    m_nNumberOfConnections;
    bool            m_bVerbose;
};

} //end of namespace
#endif
