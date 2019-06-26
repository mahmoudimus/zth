#include <libzth/config.h>
#include <libzth/io.h>
#include <libzth/util.h>
#include <libzth/worker.h>

#ifdef ZTH_HAVE_POLL
#  include <alloca.h>
#  include <fcntl.h>
#  ifndef POLLIN_SET
#    define POLLIN_SET (POLLRDBAND | POLLIN | POLLHUP | POLLERR)
#  endif
#  ifndef POLLOUT_SET
#    define POLLOUT_SET (POLLWRBAND | POLLOUT | POLLERR)
#  endif
#  ifndef POLLEX_SET
#    define POLLEX_SET (POLLPRI)
#  endif
#endif

#ifdef ZTH_CONFIG_WRAP_IO
#  include <dlfcn.h>
#endif

namespace zth { namespace io {

ssize_t (*real_read)(int,void*,size_t) = &::read;
int (*real_select)(int,fd_set*,fd_set*,fd_set*,struct timeval*) = &::select;
int (*real_poll)(struct pollfd*,nfds_t,int) = &::poll;

#ifdef ZTH_HAVE_POLL
ssize_t read(int fd, void* buf, size_t count) {
	int flags = fcntl(fd, F_GETFL);
	if(unlikely(flags == -1))
		return -1; // with errno set

	if((flags & O_NONBLOCK)) {
		zth_dbg(io, "[%s] read(%d) non-blocking", currentFiber().str().c_str(), fd);
		// Just do the call.
		return real_read(fd, buf, count);
	}

	while(true) {
		struct pollfd fds = {};
		fds.fd = fd;
		fds.events = POLLIN_SET;
		switch(real_poll(&fds, 1, 0)) {
		case 0: {
			// No data, so the read() would block.
			// Forward our request to the Waiter.
			zth_dbg(io, "[%s] read(%d) hand-off", currentFiber().str().c_str(), fd);
			AwaitFd w(&fds, 1);
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
			return real_read(fd, buf, count);
		
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

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	if(nfds < 0) {
		errno = EINVAL;
		return -1;
	}
	if(nfds == 0)
		return 0;
	if(timeout && timeout->tv_sec == 0 && timeout->tv_usec == 0)
		// Non-blocking call.
		return real_select(nfds, readfds, writefds, exceptfds, timeout);

	zth_dbg(io, "[%s] select(%d) hand-off", currentFiber().str().c_str(), nfds);

	// Convert select() to poll()
	struct pollfd* fds_heap = NULL;
	struct pollfd* fds = NULL;
	if(nfds < 16)
		fds = (struct pollfd*)alloca(sizeof(struct pollfd*) * nfds * 3);
	else if(!(fds = fds_heap = (struct pollfd*)malloc(sizeof(struct pollfd*) * nfds * 3))) {
		errno = ENOMEM;
		return -1;
	}

	int nfds_poll = 0;
	for(int fd = 0; fd < nfds; fd++) {
		if(readfds)
			if(FD_ISSET(fd, readfds)) {
				fds[nfds_poll].fd = fd;
				fds[nfds_poll].events = POLLIN_SET;
				nfds_poll++;
			}
		if(writefds)
			if(FD_ISSET(fd, writefds)) {
				fds[nfds_poll].fd = fd;
				fds[nfds_poll].events = POLLOUT_SET;
				nfds_poll++;
			}
		if(exceptfds)
			if(FD_ISSET(fd, exceptfds)) {
				fds[nfds_poll].fd = fd;
				fds[nfds_poll].events = POLLEX_SET;
				nfds_poll++;
			}
	}

	int res = 0;
	Timestamp t;
	if(timeout)
		t = Timestamp::now() + TimeInterval(timeout->tv_sec, (long)timeout->tv_usec * 1000L);

	AwaitFd w(fds, nfds_poll, t);
	if(currentWorker().waiter().waitFd(w))
		// Some error;
		goto done;

	// Success, convert poll() outcome to select() outcome
	if(readfds)
		FD_ZERO(readfds);
	if(writefds)
		FD_ZERO(writefds);
	if(exceptfds)
		FD_ZERO(exceptfds);
	
	for(int i = 0; i < nfds_poll; i++) {
		if(readfds && (fds[i].revents & POLLIN_SET)) {
			FD_SET(i, readfds);
			res++;
		}
		if(writefds && (fds[i].revents & POLLOUT_SET)) {
			FD_SET(i, writefds);
			res++;
		}
		if(exceptfds && (fds[i].revents & POLLEX_SET)) {
			FD_SET(i, exceptfds);
			res++;
		}
	}

done:
	if(fds_heap)
		free(fds_heap);
	if(w.error()) {
		errno = w.error();
		return -1;
	}
	return res;
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
	if(timeout == 0)
		// Non-blocking poll.
		return real_poll(fds, nfds, 0);
	
	zth_dbg(io, "[%s] poll(%d) hand-off", currentFiber().str().c_str(), (int)nfds);
	Timestamp t;
	if(timeout > 0)
		t = Timestamp::now() + TimeInterval(timeout / 1000, (long)(timeout % 1000) * 1000000L);
	
	AwaitFd w(fds, nfds, t);
	if(currentWorker().waiter().waitFd(w)) {
		// Some error;
		errno = w.error();
		return -1;
	}

	return w.result();
}
#endif

} } // namespace

#ifdef ZTH_CONFIG_WRAP_IO
static void zth_init_io() {
	if(!(zth::io::real_read = (decltype(zth::io::real_read))dlsym(RTLD_NEXT, "read")))
		goto error;
	if(!(zth::io::real_select = (decltype(zth::io::real_select))dlsym(RTLD_NEXT, "select")))
		goto error;
	if(!(zth::io::real_poll = (decltype(zth::io::real_poll))dlsym(RTLD_NEXT, "poll")))
		goto error;
	return;

error:
	fprintf(stderr, "Cannot load IO functions; %s\n", dlerror());
	exit(1);
}
INIT_CALL(zth_init_io)

ssize_t read(int fd, void* buf, size_t count) {
	if(zth::Config::WrapIO)
		return zth::io::read(fd, buf, count);
	else
		return zth::io::real_read(fd, buf, count);
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
	if(zth::Config::WrapIO)
		return zth::io::select(nfds, readfds, writefds, exceptfds, timeout);
	else
		return zth::io::real_select(nfds, readfds, writefds, exceptfds, timeout);
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
	if(zth::Config::WrapIO)
		return zth::io::poll(fds, nfds, timeout);
	else
		return zth::io::real_poll(fds, nfds, timeout);
}
#endif

