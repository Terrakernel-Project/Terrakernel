#ifndef EXEC_HPP
#define EXEC_HPP 1

namespace proc::exec {

int execve(const char *pathname, const char* argv[], const char* envp[]);

}

#endif
