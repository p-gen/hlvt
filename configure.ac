#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

m4_define(my_version, [m4_esyscmd([tr -d '\n' < version])])

AC_PREREQ([2.69])
AC_INIT([hlvt], [my_version], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([hlvt.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([no-dist-gzip dist-bzip2 foreign -Wall])
AM_SILENT_RULES([yes])


# Checks for programs.
AC_PROG_CC
AC_PATH_PROGS([RUBY], [ruby ruby20 ruby21 ruby22 ruby23 ruby24])

if test "x$RUBY" = x; then
  AC_MSG_WARN([ruby was not found, use the pre-generated files])
  if test -f vtparse_table.h -a \
          -f vtparse_table.c; then
    AC_MSG_NOTICE([vtparse_table.h and .c are already there, nothing to do])
  else
    AC_MSG_NOTICE([copy the pre-generated vtparse_table.*.pre files])
    cp vtparse_table.h.pre vtparse_table.h
    cp vtparse_table.c.pre vtparse_table.c
  fi
else
  AC_MSG_NOTICE([remove the possibly existing vtparse_table.h and .c files])
  rm -f vtparse_table.h vtparse_table.c
fi

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CONFIG_FILES([Makefile])
AC_OUTPUT([hlvt.spec])
