//systable.h
//System call table as macro-trick
//Bryan E. Topp <betopp@betopp.com> 2021

//Define these symbols and then include this file for per-system-call behavior.
//SYSCALLxR, SYSCALLxV, SYSCALLxN are for system calls with x parameters.
//R means returns something, V means returns void. N means doesn't return.

SYSCALL0V(0x00, void,     _sc_none        )
SYSCALL2N(0x01, void,     _sc_exit,       int, int)
SYSCALL1R(0x02, int,      _sc_panic,      const char*)
SYSCALL0R(0x03, pid_t,    _sc_getpid      )
SYSCALL0R(0x04, pid_t,    _sc_getppid     )
SYSCALL1R(0x05, pid_t,    _sc_getpgid,    pid_t)
SYSCALL2R(0x06, pid_t,    _sc_setpgid,    pid_t, pid_t)
SYSCALL3R(0x07, int,      _sc_getrlimit,  int, _sc_rlimit_t *, size_t)
SYSCALL3R(0x08, int,      _sc_setrlimit,  int, const _sc_rlimit_t *, size_t)
SYSCALL0R(0x09, pid_t,    _sc_fork        )
SYSCALL3R(0x0A, int,      _sc_exec,       int, char * const *, char * const *)
SYSCALL2R(0x0B, int,      _sc_find,       int, const char *)
SYSCALL4R(0x0C, int,      _sc_create,     int, const char *, mode_t, int)
SYSCALL3R(0x0D, int,      _sc_access,     int, int, int)
SYSCALL3R(0x0E, ssize_t,  _sc_read,       int, void *, ssize_t)
SYSCALL3R(0x0F, ssize_t,  _sc_write,      int, const void *, ssize_t)
SYSCALL3R(0x10, off_t,    _sc_seek,       int, off_t, int)
SYSCALL2R(0x11, off_t,    _sc_trunc,      int, off_t)
SYSCALL4R(0x12, int,      _sc_unlink,     int, const char *, int, int)
SYSCALL3R(0x13, int,      _sc_flag,       int, int, int)
SYSCALL1R(0x14, int,      _sc_close,      int)
SYSCALL1R(0x15, int,      _sc_chdir,      int)
SYSCALL3R(0x16, int,      _sc_dup,        int, int, bool)
SYSCALL3R(0x17, ssize_t,  _sc_stat,       int, _sc_stat_t *, size_t)
SYSCALL4R(0x18, int,      _sc_ioctl,      int, int, void *, size_t)
SYSCALL3R(0x19, ssize_t,  _sc_sigmask,    int, _sc_sigset_t *, ssize_t)
SYSCALL2R(0x20, ssize_t,  _sc_sigsuspend, const _sc_sigset_t *, ssize_t)
SYSCALL3R(0x21, int,      _sc_sigsend,    int, int, int)
SYSCALL2R(0x22, ssize_t,  _sc_siginfo,    _sc_siginfo_t *, ssize_t)
SYSCALL1R(0x24, int,      _sc_nanosleep,  int64_t)
SYSCALL3R(0x25, int,      _sc_rusage,     int, _sc_rusage_t *, size_t)
SYSCALL2R(0x26, intptr_t, _sc_mem_avail,  intptr_t, size_t)
SYSCALL3R(0x27, int,      _sc_mem_anon,   uintptr_t, size_t, int)
SYSCALL5R(0x28, ssize_t,  _sc_wait,       int, pid_t, int, _sc_wait_t *, size_t)
SYSCALL3R(0x29, int,      _sc_priority,   int, int, int)
SYSCALL0R(0x30, int64_t,  _sc_getrtc      )

SYSCALL0V(0x50, void,     _sc_pause       )

SYSCALL2R(0x60, int,      _sc_win_new,    const _sc_win_parms_t *, ssize_t)
SYSCALL3R(0x61, int,      _sc_win_up,     int, const void *, _sc_win_rect_t)
SYSCALL2R(0x62, ssize_t,  _sc_win_evt,    _sc_win_evt_t *, ssize_t)
