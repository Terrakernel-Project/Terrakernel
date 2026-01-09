#ifndef SYS_NUM_HPP
#define SYS_NUM_HPP 1

#define neu(name, num) constexpr uint64_t SYS_##name = num;
// using german neu because new is a keyword

neu(read,0)
neu(write,1)
neu(open,2)
neu(close,3)
neu(stat,4)
neu(fstat,5)
neu(lstat,6)
neu(brk,12)
neu(dup,32)
neu(dup2,33)
neu(nanosleep,35)
neu(getpid,39)
neu(fork,57)
neu(vfork,58)
neu(execve,59)
neu(exit,60)
neu(truncate,76)
neu(ftruncate,77)
neu(rename,82)
neu(mkdir,83)
neu(rmdir,84)
neu(reboot,169)

#endif
