#include <libzth/io.h>
#include <libzth/util.h>
#include <libzth/worker.h>

#ifndef ZTH_OS_WINDOWS
#  include <sys/select.h>
#  include <fcntl.h>
#endif

namespace zth { namespace io {

#ifndef ZTH_OS_WINDOWS
ssize_t read(int fd, void* buf, size_t count) {
	int flags = fcntl(fd, F_GETFL);
	if(unlikely(flags == -1))
		return -1; // with errno set

	if((flags & O_NONBLOCK)) {
		zth_dbg(io, "[%s] read(%d) non-blocking", currentFiber().str().c_str(), fd);
		// Just do the call.
		return ::read(fd, buf, count);
	}

	while(true) {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		struct timeval tv = {};
		switch(select(fd + 1, &fds, NULL, NULL, &tv)) {
		case 0: {
			// No data, so the read() would block.
			// Forward our request to the Waiter.
			zth_dbg(io, "[%s] read(%d) hand-off", currentFiber().str().c_str(), fd);
			AwaitFd w(fd, AwaitFd::AwaitRead);
			if(currentWorker().waiter().waitFd(w)) {
				// Got some error.
				errno = w.error();
				return -1;
			}
			// else: fall-through to go read the data.
		}
		case 1:
			// Got data to read.
			zth_dbg(io, "[%s] read(%d)", currentFiber().str().c_str(), fd);
			return ::read(fd, buf, count);
		
		default:
			// Huh?
			zth_assert(false);
			errno = EINVAL;
			// fall-through
		case -1:
			// Error. Return with errno set.
			return -1;
		}
	}
}
#endif

} } // namespace
