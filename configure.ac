AC_INIT([simplechatserver],[0.1],[joe@manvscode.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])


AC_PROG_CC
AC_PROG_CXX

AC_PROG_INSTALL
	
CFLAGS="-Wall -g -O0 -D_DEBUG -D_PROTOCOL_DEBUG"
CXXFLAGS=$CFLAGS

AC_CHECK_LIB([pthread], [pthread_create])

AC_CHECK_HEADERS([pthread.h])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([
   Makefile
   src/Makefile
])
AC_OUTPUT

