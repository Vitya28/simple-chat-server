#ifndef _USER_H_
#define _USER_H_

#include <string>
#include <list>
#include <set>

namespace SCS {

class User 
{
  public:
	typedef std::set<std::string> ChatroomCollection;

	explicit User( int userSocket );
	explicit User( int userSocket, const std::string &username, const std::string &ip );
	virtual ~User( );
  
  	int socket( ) const;
	std::string &username( );
	const std::string &username( ) const;
	std::string &ipAddress( );
	const std::string &ipAddress( ) const;
	void addChatroom( const std::string &chatroomName );
	void removeChatroom( const std::string &chatroomName );
  	ChatroomCollection &chatrooms( );
  	const ChatroomCollection &chatrooms( ) const;
  
  protected:
  	int m_UserSocket; // the client socket
	std::string m_UserName;
	std::string m_IPAddress;
	ChatroomCollection m_Chatrooms;
};

inline int User::socket( ) const
{ return m_UserSocket; }

inline std::string &User::username( )
{ return m_UserName; }

inline const std::string &User::username( ) const
{ return m_UserName; }

inline std::string &User::ipAddress( )
{ return m_IPAddress; }

inline const std::string &User::ipAddress( ) const
{ return m_IPAddress; }

inline User::ChatroomCollection &User::chatrooms( )
{ return m_Chatrooms; }

inline const User::ChatroomCollection &User::chatrooms( ) const
{ return m_Chatrooms; }

/*
 *	How to compare two User objects...
 */
inline bool operator<( const User &u1, const User &u2 )
{ return u1.socket( ) < u2.socket( ); }

inline bool operator==( const User u1, const User &u2 )
{ return u1.socket( ) == u2.socket( ); }


} // end of namespace SCS
#endif
