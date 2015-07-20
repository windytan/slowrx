AC_INIT([slowrx], [0.1], [oona@kapsi.fi])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AX_CXX_COMPILE_STDCXX_11()
AC_PROG_CXX
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/uus/Makefile
])
PKG_CHECK_MODULES([GTKMM], [gtkmm-3.0 >= 3.8.0])
AC_OUTPUT
