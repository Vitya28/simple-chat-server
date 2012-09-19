/*
 *	main.cc
 * 	
 *	Entry-point of program.
 *	Coded by Joseph A. Marrero
 */
#include <cassert>
#include <cerrno>
#include <clocale>
#include <sstream>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#ifndef WIN32
#include <fcntl.h>
#include <unistd.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <syslog.h>
#endif
#include "main.h"
#include "engine.h"
#include "simplechatserver.h"

using namespace std;
using namespace SCS;


bool bVerbose                = false;
bool bLoggingEnabled         = false;
unsigned short nPort         = SimpleChatServer::DEFAULT_PORT;
unsigned int nMaxConnections = 100;
unsigned int nMaxChatrooms   = 100;
bool bDaemonMode             = false;

enum DaemonAction {
    START,
    RESTART,
    SHUTDOWN
};	

int main( int argc, char *argv[] )
{
	DaemonAction action = START;	

	// read in command line arguments...
	for( int arg = 1; arg < argc; arg++ )
	{
		if( !strcmp( argv[ arg ], "--verbose" ) || !strcmp( argv[ arg ], "-v" ) )
			bVerbose = true;
		else if( !strcmp( argv[ arg ], "--enable-logging" ) || !strcmp( argv[ arg ], "-l" ) )
			bLoggingEnabled = true;
		else if( !strcmp( argv[ arg ], "--port" ) || !strcmp( argv[ arg ], "-p" ) )
			nPort = atoi( argv[ ++arg ] );		
		else if( !strcmp( argv[ arg ], "--max-connections" ) || !strcmp( argv[ arg ], "-m" ) )
			nMaxConnections = atoi( argv[ ++arg ] );		
		else if( !strcmp( argv[ arg ], "--max-chatrooms" ) || !strcmp( argv[ arg ], "-c" ) )
			nMaxChatrooms = atoi( argv[ ++arg ] );
		else if( !strcmp( argv[ arg ], "--daemon" ) || !strcmp( argv[ arg ], "-D" ) )
		{
			bDaemonMode = true;
			arg++;
			if( arg < argc )
			{
				if( !strcmp( argv[ arg ], "start" ) )
					action = START;
				else if( !strcmp( argv[ arg ], "restart" ) )
					action = RESTART;
				else if( !strcmp( argv[ arg ], "shutdown" ) )
					action = SHUTDOWN;
				else
					cerr << SCS_ERROR_HEADER << argv[ arg - 1 ] << " option expects to be followed by [start | restart | shutdown]" << endl;
			}
			else
				cerr << SCS_ERROR_HEADER << argv[ arg - 1 ] << " option expects to be followed by [start | restart | shutdown]" << endl;
		}
		else 
		{
			if( !strcmp( argv[ arg ], "--help" ) || !strcmp( argv[ arg ], "-h" ) ) // this should be last
			{
				about( argv[ 0 ] );
				return EXIT_SUCCESS;
			}
			else
			{
				cerr << SCS_ERROR_HEADER << "Unknown option given." << endl;
				about( argv[ 0 ] );
				return EXIT_FAILURE;
			}
		}
	}


	bool bReturn = false;

	switch( action )
	{
		case START:
			bReturn = start( );
			break;
		case RESTART:
			bReturn = restart( );
			break;
		case SHUTDOWN:
			bReturn = shutdown( );
			break;
		default:
			bReturn = start( );
			break;		
	}

	return bReturn ? EXIT_SUCCESS : EXIT_FAILURE;
}



/*
 *	Here we have to check if an instance of the daemon
 *	is already running. If so, we do nothing. Otherwise,
 *	we start it.
 */
bool start( )
{
	#ifndef _DEBUG	
    if( bDaemonMode )
    {
		pid_t p = fork( );

		if( p < 0 ) // on error
		{
			cerr << SCS_ERROR_HEADER << strerror( errno ) << endl;
			exit( EXIT_FAILURE );
		}
		else if( p > 0 ) // parent exits...
		{
			exit( EXIT_SUCCESS );
		}

		if( setsid( ) < 0 )
		{
			perror( SCS_ERROR_HEADER ); 
			exit( EXIT_FAILURE );
		}	

		umask( 027 );
		chdir( "/" );

		int lfp = open( LOCK_FILE, O_RDWR | O_CREAT, 0640 );
		if( lfp < 0 )
		{
			cerr << SCS_ERROR_HEADER << strerror( errno ) << endl;
			exit( EXIT_FAILURE ); /* can not open */
		}

		// Not supported with Cygwin	
		//if( lockf( lfp, F_TLOCK, 0 ) < 0 )
		//{ 
		//	cerr << SCS_ERROR_HEADER << strerror( errno ) << endl;
		//	exit( EXIT_FAILURE ); /* can not lock */
		//}

		/* only first instance continues */

		ostringstream osPID;
		osPID << getpid( ) << endl;
		string strPID = osPID.str( );

		write( lfp, strPID.c_str( ), strPID.length( ) ); /* record pid to lockfile */
    }


    //for( int i = getdtablesize( ); i >= 0; --i ) close( i ); /* close all descriptors */

	if( !bVerbose )
	{
		/* Close out the standard file descriptors */
		close( STDIN_FILENO );
		close( STDOUT_FILENO );
		close( STDERR_FILENO );
	}
	else
	{
		close( STDIN_FILENO );		
	}
	#endif	


	SCS::Engine *eng = SCS::Engine::getInstance( );
    assert( eng != NULL );

    eng->setVerbose( bVerbose );
    eng->setLogging( bLoggingEnabled );
    eng->setPort( nPort );
    eng->setMaxConnections( nMaxConnections );
    eng->setMaxChatrooms( nMaxChatrooms );

	#ifndef WIN32
    signal( SIGPIPE, engineSignalHandler );	
    signal( SIGSTOP, engineSignalHandler );	
    signal( SIGHUP, engineSignalHandler ); /* catch hangup signal */
    signal( SIGTERM, engineSignalHandler ); /* catch kill signal */
	#endif

    // start the server engine...
    eng->go( );
    return true;
}

/*
 *	Here we have to check if an instance of the daemon
 *	is running. If not, we start it. Otherwise,
 *	we restart it.
 */
bool restart( )
{
    long lfp = open( LOCK_FILE, O_RDONLY, 0640 );

    if( lfp < 0 )
	{
		return start( ); // file did not exist so start the daemon.
	}

    char buffer[ 8 ]; // safe

    read( lfp, buffer, sizeof(char) * 8 );

    char *endptr = NULL;
    long thePID = strtol( buffer, &endptr, 0 );


    if( thePID == 0 )
    {
		cerr << SCS_ERROR_HEADER << " Could not extract PID from the lock file." << endl;
		return false;
    }

    kill( thePID, SIGHUP );
    return true;
}

/*
 *	Here we have to check if an instance of the daemon
 *	is running. If not, we do nothing. Otherwise,
 *	we shut it down.
 */
bool shutdown( )
{
	long lfp = open( LOCK_FILE, O_RDONLY, 0640 );

	if( lfp < 0 )
	{
		cerr << SCS_ERROR_HEADER << "Problem with lock file; " << strerror(errno) << endl;
		return true;
	}

	char buffer[ 8 ];

	read( lfp, buffer, sizeof(char) * 8 );

	char *endptr = NULL;
	long thePID = strtol( buffer, &endptr, 0 );

	if( thePID == 0 )
	{
		cerr << SCS_ERROR_HEADER << " Could not extract PID from the lock file." << endl;
		return false;
	}

	kill( thePID, SIGTERM );
	remove( LOCK_FILE );
	return true;
}

void about( const char *progName )
{
    cout << DAEMON_NAME << ", Release v" << RELEASE_VER << endl;
    cout << "Copyright (c) 2007, Joseph A. Marrero. All rights reserved." << endl;
    cout << "More information at http://www.ManVsCode.com/" << endl << endl;

    cout << "The syntax is: " << endl;
    cout << progName << " [ OPTIONS ]" << endl << endl;

    cout << "Options:" << endl;	
    //cout << setw(2) << "" << setw(25) << left << "-f, --config-file F"				<< setw(40) << "Uses the config file F" << endl;
    cout << setw(2) << "" << setw(25) << left << "-p, --port N"				<< setw(40) << "Sets the port number to N." << endl;
    cout << setw(2) << "" << setw(25) << left << "-m, --max-connections N" 	<< setw(40) << "Sets the maximum concurrent connections to N." << endl;
    cout << setw(2) << "" << setw(25) << left << "-c, --max-chatrooms N" 	<< setw(40) << "Sets the max chatrooms to N." << endl;
    cout << setw(2) << "" << setw(25) << left << "-v, --verbose"			<< setw(40) << "Turn on extra messages and echo to stdout." << endl;
    cout << setw(2) << "" << setw(25) << left << "-l, --enable-logging" 	<< setw(40) << "Turn on logging; this decreases performance." << endl;
	#ifndef WIN32
    cout << setw(2) << "" << setw(25) << left << "-D, --daemon ACTION"<< setw(40) << "Run as daemon; ACTION is either start, restart, or shutdown." << endl;
	#endif

    cout << setw(2) << "" << setw(25) << left << "-h, --help" 				<< setw(40) << "Display help and copyright information." << endl;

    cout << endl;
    cout << "Please report any bugs to joe@manvscode.com" << endl;
}
