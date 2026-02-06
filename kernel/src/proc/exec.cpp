#include "exec.hpp"
#include <cstdio>
#include <ramfs/ramfs.hpp>
#include <mem/mem.hpp>
#include <exec/elf.hpp>

namespace proc::exec {

int execve(const char *pathname, const char* argv[], const char* envp[]) {
	int fd = ramfs::open(pathname, O_RDONLY);
	if (fd < 0) return -1;

	stat s; // cuz i dont want to use fseek, fstat is kewler
	ramfs::fstat(fd, &s);

	if (s.st_size <= 0) return -1; // attempting to run a 0- sized file? schade... DU KANNST NICHT!!!!!!!!!

	void* exe_buf = mem::heap::malloc(s.st_size);
	if (!exe_buf) return -1; // no memory, ugly, btw, malloc will panic by itself
							 // so this return -1 is just a trust issue

	if (ramfs::read(fd, exe_buf, s.st_size) != s.st_size) return -1; // couldn't read entire file...

	run_elf(exe_buf, s.st_size, true, argv, envp);

	return 0; // to shut up gcc but we all know NO RETURN MUAHAHAHA
}

}
