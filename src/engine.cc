/*
 *	engine.cc
 *
 *	The Engine class manages the state of the daemon.
 *
 *	Coded by Joseph A. Marrero
 *	9/2/07
 */

#include <cassert>
#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#ifndef WIN32
#include <syslog.h>
#endif
#include "engine.h"
#include "main.h"


namespace SCS {

Engine *Engine::m_pInstance = NULL;


Engine::Engine( )
  : m_bVerbose(false), 
    m_bLogginEnabled(false), 
    m_usPort(0),
    m_nMaxConnections(0),
    m_nMaxChatrooms(0),
    m_pServer(NULL)
{
}

Engine::Engine( const Engine& engine )
  : m_bVerbose(false), 
    m_bLogginEnabled(false), 
    m_usPort(0),
    m_nMaxConnections(0),
    m_nMaxChatrooms(0),
    m_pServer(NULL)
{	
    assert(false); // not implemented...
}

Engine::~Engine( )
{
}

Engine *Engine::getInstance( )
{
    if( !m_pInstance )
	{
		m_pInstance = new Engine( );
	}

    return m_pInstance;
}

bool Engine::initialize( )
{
	#ifndef WIN32
    openlog( SCS_LOG_ID, LOG_PID, LOG_DAEMON );
	#endif

    /*
     * Set up logging...
     */
    m_pServer =  SimpleChatServer::getInstance( );

    Engine::onInfo( "Starting..." );

    return m_pServer->initialize( getMaxChatrooms( ), 100, getPort( ), getMaxConnections( ) );
}

bool Engine::deinitialize( )
{
    Engine::onInfo( "Deinitializing..." );

    if( !m_pServer->deinitialize( ) )
	{
		return false;
	}

    Engine::onInfo( "Engine deinitialization complete." );

    delete m_pServer;
    return true;
}


Engine &Engine::operator= ( const Engine &engine ) // private 
{
    return *this; 
}

void Engine::go( )
{
	if( !initialize( ) )
	{
		Engine::onError( "Initialization failed during startup!" );
		exit( EXIT_FAILURE );
	}

	while( true ) 
	{
		int clientSocket = m_pServer->acceptConnection( );
		if( clientSocket > 0 ) m_pServer->handleClient( clientSocket );
	}
	////////////////////////////////////////////////////
	///////////////// NEVER REACHED ////////////////////
	////////////////////////////////////////////////////
}

void Engine::restart( )
{
	Engine::onInfo( "Restarting..." );

	if( !deinitialize( ) )
	{
		Engine::onError( "An error has occured when attempting to deinitialize during restart." ); 
		// TO DO...
		// ERROR Handling
	}
	else {
		initialize( );
		Engine::onInfo( "Restarting Complete." );
	}

}

void Engine::shutdown( )
{
	Engine::onInfo( "Shutting down..." );

	if( !deinitialize( ) )
	{
		Engine::onError( "Deinitialization failed during shutdown!" );
		exit( EXIT_FAILURE );
	}

	Engine::onInfo( "Shutdown complete." );
	#ifndef WIN32
	closelog( );
	#endif
	delete this; // looks weird, but safe.
	exit( EXIT_SUCCESS );
}




void Engine::onError( const char *pErrorMessageFormat, ... )
{
	va_list args;

	if( m_pInstance->isVerboseEnabled( ) )
	{
		va_start( args, pErrorMessageFormat );
		printf( SCS_ERROR_HEADER );
		vprintf( pErrorMessageFormat, args );

		#ifdef WIN32
		int error = WSAGetLastError( );
		#endif


		printf( "\n" );
		va_end( args ); //invalidates args
	}

	if( m_pInstance->isLoggingEnabled( ) ) // Log the error...
	{
		#ifndef WIN32
		va_start( args, pErrorMessageFormat ); //reinitialize args
		vsyslog( LOG_ERR, pErrorMessageFormat, args );
		va_end( args );
		#endif
	}
}

void Engine::onInfo( const char *pInfoMessageFormat, ... )
{
	va_list args;

	if( m_pInstance->isVerboseEnabled( ) )
	{
		va_start( args, pInfoMessageFormat );
		printf( SCS_INFO_HEADER );
		vprintf( pInfoMessageFormat, args );
		printf( "\n" );
		va_end( args ); //invalidates args
	}
	
	if( m_pInstance->isLoggingEnabled( ) ) // Log the alert...
	{
		#ifndef WIN32
		va_start( args, pInfoMessageFormat ); //reinitialize args
		vsyslog( LOG_INFO, pInfoMessageFormat, args );
		va_end( args );
		#endif
	}
}

////////////////////////////////////////////////////////////////////
///////////////////////// SIGNAL HANDLER /////////////////////////// 
//////////////////////////////////////////////////////////////////// 
#ifndef WIN32
void engineSignalHandler( int signal )
{
    static Engine *engine = Engine::getInstance( );

    switch( signal )
    {
		case SIGHUP:
			engine->restart( );
			break;
		case SIGSTOP:
			engine->shutdown( );
			break;
		case SIGTERM:
			engine->shutdown( );
			break;
		case SIGPIPE:
			break;
		default:
			//Engine::onInfo( "Received a signal that I do not know how to handle. Signal = %d", signal );
			break;
    }
}
#endif

} //end of namespace
