AC_INIT(esesc, 2, renau@soe.ucsc.edu luisceze@cs.uiuc.edu)

AC_DEFUN([AC_COMPOPT],
AC_SUBST([$1]_OPT,[[[$1]=1]]))

AC_DEFUN([AC_NOCOMPOPT],
AC_SUBST([[$1]_OPT],[[[#$1]=1]]))


AC_SUBST(BUILD_DIR, `pwd`)
AC_SUBST(SRC_DIR, `pwd`/$srcdir/src)
AC_SUBST(TOPSRC_DIR, `pwd`/$srcdir)

AC_CONFIG_FILES([Makefile:src/Makefile.in] 
	  	[Make.defs:src/Make.defs.in])

#default simulator executable to be built
AC_SUBST(DEFEXEC,[sesc.mem])
AC_SUBST(DEFCONF,[mem.conf])

#declare compilation options here
AC_SUBST(TASKSCALAR_OPT)
AC_SUBST(VALUEPRED_OPT)
AC_SUBST(SESC_ENERGY_OPT)
AC_SUBST(SESC_MISPATH_OPT)
AC_SUBST(TS_VMEM_OPT)
AC_SUBST(DEBUG_OPT)
AC_SUBST(DEBUG_SILENT_OPT)
AC_SUBST(DIRECTORY_OPT)
AC_SUBST(TS_PROFILING_OPT)
AC_SUBST(TS_RISKLOADPROF_OPT)
AC_SUBST(NO_MERGELAST_OPT)
AC_SUBST(TS_SPAWN_OPT)
AC_SUBST(SESC_SLICE_OPT)
AC_SUBST(NO_MERGENEXT_OPT)
AC_SUBST(ATOMIC_OPT)
AC_SUBST(MPCOH_OPT)
AC_SUBST(SESC_DSM_OPT)

#we are not defining compiler options yet, but we should
#AC_PROG_CC

#### put the configure script options macros here

###TASKSCALAR
AC_NOCOMPOPT(TASKSCALAR)
AC_NOCOMPOPT(VALUEPRED)
AC_ARG_ENABLE(taskscalar, 
AC_HELP_STRING([--enable-taskscalar],
               [Enable TaskScalar TLS support (default is no)]),
[if test "$enableval" = "yes"; then 
    AC_COMPOPT(TASKSCALAR) AC_COMPOPT(VALUEPRED)
fi],
)

###SESC_ENERGY

AC_NOCOMPOPT(SESC_ENERGY)
AC_ARG_ENABLE(power, 
AC_HELP_STRING([--enable-power],
               [Enable power model (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_ENERGY) fi],
)


###SESC_MISPATH

AC_NOCOMPOPT(SESC_MISPATH)
AC_ARG_ENABLE(mispath, 
AC_HELP_STRING([--enable-mispath],
               [Enable simulation of wrong-path instructions (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_MISPATH) fi],
)


###DEBUG

AC_NOCOMPOPT(DEBUG)
AC_ARG_ENABLE(debug, 
AC_HELP_STRING([--enable-debug],
               [Enabes assertions and LOG messages (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(DEBUG) fi],
)

###DEBUG_SILENT

AC_NOCOMPOPT(DEBUG_SILENT)
AC_ARG_ENABLE(debug-silent, 
AC_HELP_STRING([--enable-debug-silent],
               [In debug mode, do not show LOG messages (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(DEBUG_SILENT) AC_COMPOPT(DEBUG) fi],
)


###DIRECTORY

AC_NOCOMPOPT(DIRECTORY)
AC_ARG_ENABLE(vmem-directory, 
AC_HELP_STRING([--enable-vmem-directory],
               [Enables traffic filtering in VMEM (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(DIRECTORY) AC_COMPOPT(DIRECTORY2) fi],
)

###VMEM

AC_NOCOMPOPT(TS_VMEM)
AC_ARG_ENABLE(vmem, 
AC_HELP_STRING([--enable-vmem],
               [Enables versioning memory subsystem modeling (default is no)]),
[if test "$enableval" = "yes"; then 
    AC_COMPOPT(TS_VMEM) 
    AC_COMPOPT(TASKSCALAR) AC_COMPOPT(VALUEPRED) AC_SUBST(DEFEXEC,[sesc.tls]) AC_SUBST(DEFCONF,[vmem.conf]) 
fi],
)

###TS_PROFILING

AC_NOCOMPOPT(TS_PROFILING)
AC_ARG_ENABLE(ts-prof, 
AC_HELP_STRING([--enable-ts-prof],
               [Enables TaskScalar task profiling (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(TS_PROFILING) fi],
)

###TS_RISKLOADPROF

AC_NOCOMPOPT(TS_RISKLOADPROF)
AC_ARG_ENABLE(ts-risk-load-prof, 
AC_HELP_STRING([--enable-ts-risk-load-prof],
               [Enables TaskScalar Risk-loads profiling (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(TS_RISKLOADPROF) fi],
)

###NO_MERGELAST

AC_NOCOMPOPT(NO_MERGELAST)
AC_ARG_ENABLE(nomlast, 
AC_HELP_STRING([--enable-ts-nomlast],
               [Enables NO_MERGELAST in TaskScalar (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(NO_MERGELAST) fi],
)

###NO_MERGENEXT

AC_NOCOMPOPT(NO_MERGENEXT)
AC_ARG_ENABLE(nomnext, 
AC_HELP_STRING([--enable-ts-nomnext],
               [Enables NO_MERGENEXT in TaskScalar (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(NO_MERGENEXT) fi],
)

###TS_SPAWN
AC_NOCOMPOPT(TS_SPAWN)
AC_ARG_ENABLE(spawn, 
AC_HELP_STRING([--enable-spawn],
               [Enables TS_SPAWN in Sesc (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(TS_SPAWN) fi],
)


###SESC_SLICE
AC_NOCOMPOPT(SESC_SLICE)
AC_ARG_ENABLE(slice, 
AC_HELP_STRING([--enable-slice],
               [Enables SESC_SLICE in Sesc (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_SLICE) fi],
)

###ATOMIC

AC_NOCOMPOPT(ATOMIC)
AC_ARG_ENABLE(atomic, 
AC_HELP_STRING([--enable-atomic],
               [Enables atomic sections in TaskScalar (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(ATOMIC) AC_DEFINE(ATOMIC) fi],
)

### Cache Coherence

AC_NOCOMPOPT(MPCOH)
AC_ARG_ENABLE(cc,
AC_HELP_STRING([--enable-cc],
	       [Enables Cache Coherence (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(MPCOH) AC_SUBST(DEFEXEC,[sesc.cc]) AC_SUBST(DEFCONF,[nkcc1.conf]) fi],
)

### Distributed Shared Memory

AC_NOCOMPOPT(SESC_DSM)
AC_ARG_ENABLE(dsm,
AC_HELP_STRING([--enable-dsm],
	       [Enables DSM System (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_DSM) AC_SUBST(DEFEXEC,[sesc.dsm]) AC_SUBST(DEFCONF,[dsm.conf]) fi],
)



AC_OUTPUT
