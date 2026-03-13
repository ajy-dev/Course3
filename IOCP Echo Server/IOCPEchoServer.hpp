#ifndef IOCP_ECHO_SERVER_HPP
# define IOCP_ECHO_SERVER_HPP

# include <cstdint>
# include <atomic>
# include <unordered_map>
# include <vector>
# include <WinSock2.h>
# include "RingBuffer.hpp"

class IOCPEchoServer
{
public:
	explicit IOCPEchoServer(std::uint16_t port);
	~IOCPEchoServer(void) noexcept;

	IOCPEchoServer(const IOCPEchoServer &other) = delete;
	IOCPEchoServer &operator=(const IOCPEchoServer &other) = delete;

	IOCPEchoServer(IOCPEchoServer &&other) = delete;
	IOCPEchoServer &operator=(IOCPEchoServer &&other) = delete;

	bool start(void);
	void shutdown(void) noexcept;
private:
	struct Session
	{
		std::uint32_t sid;
		SOCKET sock;

		OVERLAPPED recv_overlapped;
		RingBuffer recv_buffer = RingBuffer(160000);

		OVERLAPPED send_overlapped;
		RingBuffer send_buffer = RingBuffer(160000);

		volatile LONG sending;
		volatile LONG io_count = 0;

		CRITICAL_SECTION lock;
	};

	static unsigned int __stdcall worker_thread_proc(LPVOID param);
	static unsigned int __stdcall accept_thread_proc(LPVOID param);

	bool recvpost(Session *session);
	bool sendpost(Session *session);

	void decrement_io_count(Session *session);

	std::uint16_t port;
	std::atomic<bool> running;

	HANDLE iocp_handle;
	SOCKET listen_sock;

	HANDLE accept_thread;
	std::vector<HANDLE> worker_threads;

	std::unordered_map<std::uint32_t, Session *> session_map;
	std::atomic<std::uint32_t> next_sid;

	SRWLOCK session_map_lock;
};

#endif