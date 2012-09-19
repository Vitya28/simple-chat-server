#ifndef _CHATROOM_H_
#define _CHATROOM_H_

#include <iostream>
#include <string>
#include <functional>
#include <list>
#include <set>
#include "protocol.h"

namespace SCS {

class Chatroom 
{
  public:
	typedef std::list<int> UserSocketCollection;

    explicit Chatroom( const std::string &name = "" );
    virtual ~Chatroom( );

	std::string &getName( );
    const std::string getName( ) const;
	
    void addUser( int userSocket );
    void removeUser( int userSocket );
  
    UserSocketCollection getUsers( ) const;  
  
    void notifyEveryone( const std::string &message, int type = NetMessaging::Protocol::MT_SERVER_CHATROOM_MESSAGE, int excludeUserSocket = -1 ) const;
    void sendMessage( int fromUserSocket, const std::string &message ) const;
  
    unsigned int getNumberOfUsers( ) const;
  
  protected:
	typedef std::set<int> SocketCollection;

	std::string m_Name;
    SocketCollection m_UserSockets;
    unsigned int m_nNumberOfUsers;
  
    void notifyEveryoneThatUserJoined( int userSocket ) const;
    void notifyEveryoneThatUserLeft( int userSocket ) const;
};


inline std::string &Chatroom::getName( )
{ return m_Name; }

inline const std::string Chatroom::getName( ) const
{ return m_Name; }

inline Chatroom::UserSocketCollection Chatroom::getUsers( ) const
{
    UserSocketCollection userList;
	SocketCollection::const_iterator itr;

    for( itr = m_UserSockets.begin( ); itr != m_UserSockets.end( ); ++itr )
		userList.push_front( *itr );

    return userList;
}

inline unsigned int Chatroom::getNumberOfUsers( ) const
{ return m_nNumberOfUsers; }

/*
 *	How to compare two Chatroom objects...
 */
inline bool operator<( const Chatroom &c1, const Chatroom &c2 )
{ return c1.getName( ) < c2.getName( ); }

inline bool operator==( const Chatroom &c1, const Chatroom &c2 )
{ return c1.getName( ) == c2.getName( ); }

/*
 * Hash function for chatrooms
 */
struct ChatroomHasher {
    size_t operator()( const Chatroom &cr ) const
    { 
		unsigned long __h = 0;
		const char *__s = cr.getName( ).c_str( );

		for ( ; *__s; ++__s)
			__h = 5 * __h + *__s;

		//cout << "ChatroomHasher(): hash code = " << size_t(__h) << endl;
		return size_t(__h);
    }
};

struct ChatroomEqual
{
    bool operator()( const Chatroom &c1, const Chatroom &c2 ) const
    { return c1.getName( ) == c2.getName( ); }
};

struct ChatroomLessThan
{
    bool operator()( const Chatroom &c1, const Chatroom &c2 ) const
    { return c1.getName( ) < c2.getName( ); }
};

} // end of namespace
#endif
