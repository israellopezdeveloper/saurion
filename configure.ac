# This is an automatically generated configuration file.

AC_INIT([Saurion],[0.0.2],[israel.lopez.developer@gmail.com])

AC_DEFINE([CHUNK_SZ], [8192], [@brief Size of chunk on I/O operations])
AC_DEFINE([ACCEPT_QUEUE], [10], [@brief Accepting queue of the socket])
AC_DEFINE([SAURION_RING_SIZE], [256], [@brief Size of liburing ring structure])
AC_DEFINE([TIMEOUT_RETRY], [10], [@brief Timeout for retrying operations])
AC_DEFINE([MAX_ATTEMPTS], [10], [@brief Number of attempts to make an operation])
AC_DEFINE([NUM_CORES], [sysconf(_SC_NPROCESSORS_ONLN)], [@brief Number of cores/processors on the computer])

# It is recommended not to modify this file from this line
# Detect the C and C++ compilers
AC_PROG_CC
AC_PROG_CXX

# Ensure that the 'ar' archiver is available
AM_PROG_AR

# Set up libtool to manage libraries
LT_INIT([static shared])

# Configure the directory where additional macros are located
AC_CONFIG_MACRO_DIRS([m4])

MY_CONFIGURATION

