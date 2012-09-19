#include "user.h"

namespace SCS {

User::User( int userSocket )
  : m_UserSocket(userSocket), m_UserName(""), m_IPAddress("")
{
}
User::User( int userSocket, const std::string &username, const std::string &ip )
  : m_UserSocket(userSocket), m_UserName(username), m_IPAddress(ip)
{
}

User::~User( )
{
}

void User::addChatroom( const std::string &chatroomName )
{ m_Chatrooms.insert( chatroomName ); }

void User::removeChatroom( const std::string &chatroomName )
{ m_Chatrooms.erase( chatroomName ); }

} // end of namespace SCS
