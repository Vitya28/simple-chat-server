#ifndef _MAIN_H_
#define _MAIN_H_
/*
 *	main.h
 * 	
 *	Entry-point of program.
 *	Coded by Joseph A. Marrero
 */

 
#define DAEMON_NAME 		"Simple Chat Server (SCS)"
#define RELEASE_VER			("0.1.0")
#define BUILD_DATETIME		( __DATE__ ", " __TIME__ )
#define DAEMON_WORKPATH		"/"
#define SCS_LOG_ID		  	"[Simple Chat Server] "
#define SCS_ERROR_HEADER 	"[ERROR] "
#define SCS_INFO_HEADER 	"[INFO] "
#define LOCK_FILE 			"/var/run/simplechatserver.pid"

bool start( );
bool restart( );
bool shutdown( );

void about( const char *progName );

#endif
