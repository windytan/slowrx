AC_INIT([slowrx], [0.1], [oona@kapsi.fi])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AX_CXX_COMPILE_STDCXX_11()
AC_PROG_CXX
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])
PKG_CHECK_MODULES([GTKMM], [gtkmm-3.0 >= 3.8.0])
PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.2, ac_cv_sndfile=1, ac_cv_sndfile=0)
AC_DEFINE_UNQUOTED([HAVE_SNDFILE],${ac_cv_sndfile},
                        [Set to 1 if you have libsndfile.])
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)
AC_OUTPUT
