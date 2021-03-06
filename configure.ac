AC_INIT(esesc, 2, renau@soe.ucsc.edu luisceze@cs.uiuc.edu)

AC_DEFUN([AC_COMPOPT],
AC_SUBST([$1]_OPT,[[[$1]=1]]))

AC_DEFUN([AC_NOCOMPOPT],
AC_SUBST([[$1]_OPT],[[[#$1]=1]]))


AC_SUBST(BUILD_DIR, `pwd`)
AC_SUBST(SRC_DIR, $srcdir/src)
AC_SUBST(TOPSRC_DIR, $srcdir)

AC_CONFIG_FILES([Makefile:src/Makefile.in] 
	  	[Make.defs:src/Make.defs.in])

#default simulator executable to be built
AC_SUBST(DEFEXEC,[sesc.mem])
AC_SUBST(DEFCONF,[mem.conf])

#declare compilation options here
AC_SUBST(TASKSCALAR_OPT)
AC_SUBST(VALUEPRED_OPT)
AC_SUBST(SESC_ENERGY_OPT)
AC_SUBST(SESC_GATHERM_OPT)
AC_SUBST(SESC_SESCTHERM_OPT)
AC_SUBST(SESC_THERM_OPT)
AC_SUBST(SESC_MISPATH_OPT)
AC_SUBST(TS_VMEM_OPT)
AC_SUBST(DEBUG_OPT)
AC_SUBST(DEBUG_SILENT_OPT)
AC_SUBST(DIRECTORY_OPT)
AC_SUBST(TS_PROFILING_OPT)
AC_SUBST(TS_RISKLOADPROF_OPT)
AC_SUBST(NO_MERGELAST_OPT)
AC_SUBST(NO_MERGENEXT_OPT)
AC_SUBST(SESC_SMP_OPT)
AC_SUBST(SESC_SMP_DEBUG_OPT)
AC_SUBST(SESC_BAAD_OPT)
AC_SUBST(CONDOR_LINK_OPT)
AC_SUBST(TRACE_DRIVEN_OPT)
AC_SUBST(SESC_RSTTRACE_OPT)
AC_SUBST(QEMU_DRIVEN_OPT)
AC_SUBST(MIPS_EMUL_OPT)
AC_SUBST(SESC_32BIT_OPT)

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

###SESC_GATHERM  (sescspot & sesctherm)

AC_NOCOMPOPT(SESC_SESCTHERM)
AC_ARG_ENABLE(gatherm, 
AC_HELP_STRING([--enable-gatherm],
               [Enable GATherm model (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_GATHERM) AC_COMPOPT(SESC_THERM) AC_COMPOPT(SESC_ENERGY) fi],
)

###SESC_THERM 

AC_NOCOMPOPT(SESC_THERM)
AC_ARG_ENABLE(therm, 
AC_HELP_STRING([--enable-therm],
               [Enable therm model (sescspot only) (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_THERM) AC_COMPOPT(SESC_ENERGY) fi],
)

###SESC_SESCTHERM  (sescspot & sesctherm)

AC_NOCOMPOPT(SESC_SESCTHERM)
AC_ARG_ENABLE(sesctherm, 
AC_HELP_STRING([--enable-sesctherm],
               [Enable SESCTherm model (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_SESCTHERM) AC_COMPOPT(SESC_THERM) AC_COMPOPT(SESC_ENERGY) fi],
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
               [Enables assertions and LOG messages (default is no)]),
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

### Symmetric MultiProcessor

AC_NOCOMPOPT(SESC_SMP)
AC_ARG_ENABLE(smp,
AC_HELP_STRING([--enable-smp],
	       [Enables SMP System (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_SMP) AC_SUBST(DEFEXEC,[sesc.smp]) AC_SUBST(DEFCONF,[smp.conf]) fi],
)

### Debugging for Symmetric MultiProcessor

AC_NOCOMPOPT(SESC_SMP_DEBUG)
AC_ARG_ENABLE(smpdebug,
AC_HELP_STRING([--enable-smpdebug],
	       [Enables SMP System advanced debug features (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_SMP_DEBUG) fi],
)

### BAAD (Bandwidth Analyzer)

AC_NOCOMPOPT(SESC_BAAD)
AC_ARG_ENABLE(baad, 
AC_HELP_STRING([--enable-baad],
               [Enables Bandwidth analyzer performance statistics (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_BAAD) fi],
)

### SESC_INORDER (optimized inorder)
AC_NOCOMPOPT(SESC_INORDER)
AC_ARG_ENABLE(inorder, 
AC_HELP_STRING([--enable-inorder],
               [Enables optimized inorder pipeline (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_INORDER) fi],
)

### Condor linking

AC_NOCOMPOPT(CONDOR_LINK)
AC_ARG_ENABLE(condor, 
AC_HELP_STRING([--enable-condor],
               [Enables linking sesc with condor libraries. Needed to run with standard universe. (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(CONDOR_LINK) 
fi],
)

### TRACE

AC_NOCOMPOPT(TRACE_DRIVEN)
AC_ARG_ENABLE(trace, 
AC_HELP_STRING([--enable-trace],
               [Enables trace-driven simulation (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(TRACE_DRIVEN) fi],
)

### RSTTRACE

AC_NOCOMPOPT(SESC_RSTTRACE)
AC_ARG_ENABLE(rsttrace, 
AC_HELP_STRING([--enable-rsttrace],
               [Enables RST trace-driven simulation (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_RSTTRACE) AC_COMPOPT(TRACE_DRIVEN) fi],
)

### QEMU

AC_NOCOMPOPT(QEMU_DRIVEN)
AC_ARG_ENABLE(qemu, 
AC_HELP_STRING([--enable-qemu],
               [Enables qemu-driven simulation (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(QEMU_DRIVEN) fi],
)

### New MIPS emulation (libemul)

AC_NOCOMPOPT(MIPS_EMUL)
AC_ARG_ENABLE(mipsemul,
AC_HELP_STRING([--enable-mipsemul],
	       [Use new MIPs emulation in libemul (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(MIPS_EMUL) fi],
)

### Build a 32-bit simulator

AC_NOCOMPOPT(SESC_32BIT)
AC_ARG_ENABLE(32bit,
AC_HELP_STRING([--enable-32bit],
	       [Build 32-bit simulator even on 64-bit host (default is no)]),
[if test "$enableval" = "yes"; then AC_COMPOPT(SESC_32BIT) fi],
)

AC_OUTPUT
