#ifndef __ENGINE_H__
#define __ENGINE_H__
/*
 *	engine.h
 *
 *	The Engine class manages the state of the daemon.
 *
 *	Coded by Joseph A. Marrero
 *	9/2/07
 */

#include <list>
#include "simplechatserver.h"



namespace SCS {


class Engine
{
  public:
    ~Engine( );
    static Engine *getInstance( );

    void go( );
    void restart( );
    void shutdown( );
	
    void setVerbose( bool bVerbose = true );
    bool isVerboseEnabled( ) const;

    void setLogging( bool on = true );
    bool isLoggingEnabled( ) const;
  
    void setPort( unsigned short port = SimpleChatServer::DEFAULT_PORT );
    unsigned short getPort( ) const;  
  
    void setMaxConnections( unsigned int maxConnections = 100 );
    unsigned short getMaxConnections( ) const;  
  
    void setMaxChatrooms( unsigned int maxChatrooms = 100 );
    unsigned short getMaxChatrooms( ) const;  
  
    static void onError( const char *pErrorMessageFormat, ... );
    static void onInfo( const char *pInfoMessageFormat, ... );
  
  private:
    Engine( );
    Engine( const Engine &engine );
  
    Engine &operator= ( const Engine &engine );
    static Engine *m_pInstance;
	
  
    bool initialize( );
    bool deinitialize( );
	
  private:
    bool m_bVerbose;
    bool m_bLogginEnabled;
    unsigned short m_usPort;
    unsigned int m_nMaxConnections;
    unsigned int m_nMaxChatrooms;
    SimpleChatServer *m_pServer;
};


inline void Engine::setVerbose( bool bVerbose )
{ m_bVerbose = bVerbose; }

inline bool Engine::isVerboseEnabled( ) const
{ return m_bVerbose; }

inline void Engine::setLogging( bool on )
{ m_bLogginEnabled = on; }

inline bool Engine::isLoggingEnabled( ) const
{ return m_bLogginEnabled; }

inline void Engine::setPort( unsigned short port )
{ m_usPort = port; }
	
inline unsigned short Engine::getPort( ) const
{ return m_usPort; }

inline void Engine::setMaxConnections( unsigned int maxConnections )
{ m_nMaxConnections = maxConnections; }

inline unsigned short Engine::getMaxConnections( ) const
{ return m_nMaxConnections; }
	
inline void Engine::setMaxChatrooms( unsigned int maxChatrooms )
{ m_nMaxChatrooms = maxChatrooms; }

inline unsigned short Engine::getMaxChatrooms( ) const
{ return m_nMaxChatrooms; }


////////////////////////////////////////////////////////////////////
///////////////////////// SIGNAL HANDLER /////////////////////////// 
//////////////////////////////////////////////////////////////////// 
#ifndef WIN32
void engineSignalHandler( int signal );
#endif

} //end of namespace
#endif
