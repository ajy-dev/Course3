#pragma comment(lib, "ws2_32")

#include "IOCPEchoServer.hpp"
#include "Logger.hpp"
#include <process.h>
#include <cstring>
#include <new>
#include <Windows.h>
#include <string.h>

static void log_system_error(const char *func_name, DWORD error_code);

IOCPEchoServer::IOCPEchoServer(std::uint16_t port)
	: port(port)
	, running(false)
	, iocp_handle(nullptr)
	, listen_sock(INVALID_SOCKET)
	, accept_thread(nullptr)
	, next_sid(0)
{
	InitializeSRWLock(&this->session_map_lock);
}

IOCPEchoServer::~IOCPEchoServer(void) noexcept
{
}

bool IOCPEchoServer::start(void)
{
	WSADATA wsa;
	SYSTEM_INFO si;
	SOCKADDR_IN addr;

	if (WSAStartup(MAKEWORD(2, 2), &wsa))
	{
		log_system_error("WSAStartup()", WSAGetLastError());
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "Winsock initialized.");

	if (wsa.wVersion != MAKEWORD(2, 2))
	{
		Logger::get_instance().log(
			Logger::LogLevel::Error,
			"WSAStartup(): requested 2.2 but got %u.%u",
			static_cast<unsigned int>(LOBYTE(wsa.wVersion)),
			static_cast<unsigned int>(HIBYTE(wsa.wVersion))
		);
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "Winsock version check passed.");

	this->iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!this->iocp_handle)
	{
		log_system_error("CreateIoCompletionPort()", GetLastError());
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "IO Completion Port created.");

	this->listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->listen_sock == INVALID_SOCKET)
	{
		log_system_error("socket()", WSAGetLastError());
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "Listen socket created.");

	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(this->port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(this->listen_sock, reinterpret_cast<const SOCKADDR *>(&addr), sizeof(addr)) == SOCKET_ERROR)
	{
		log_system_error("bind()", WSAGetLastError());
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "IP address bound to listen socket.");

	if (listen(this->listen_sock, SOMAXCONN) == SOCKET_ERROR)
	{
		log_system_error("listen()", WSAGetLastError());
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "Server listening on port %u.", static_cast<unsigned int>(this->port));

	this->running = true;

	std::uintptr_t handle = _beginthreadex(
		nullptr,
		0,
		&accept_thread_proc,
		this,
		0,
		nullptr
	);
	if (!handle)
	{
		log_system_error("_beginthreadex()", GetLastError());
		this->running = false;
		return false;
	}
	this->accept_thread = reinterpret_cast<HANDLE>(handle);
	Logger::get_instance().log(Logger::LogLevel::Info, "Accept thread initialized.");

	GetSystemInfo(&si);
	for (DWORD i = 0; i < si.dwNumberOfProcessors * 2; ++i)
	{
		std::uintptr_t handle = _beginthreadex(
			nullptr,
			0,
			&worker_thread_proc,
			this,
			0,
			nullptr
		);
		if (!handle)
		{
			log_system_error("_beginthreadex()", GetLastError());
			this->running = false;
			return false;
		}
		this->worker_threads.push_back(reinterpret_cast<HANDLE>(handle));
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "%zu worker threads initialized.", worker_threads.size());

	return true;

}

void IOCPEchoServer::shutdown(void) noexcept
{
	this->running = false;

	if (this->listen_sock != INVALID_SOCKET)
	{
		closesocket(this->listen_sock);
		this->listen_sock = INVALID_SOCKET;
	}

	for (size_t i = 0; i < this->worker_threads.size(); ++i)
		PostQueuedCompletionStatus(this->iocp_handle, 0, 0, nullptr);

	if (this->accept_thread)
	{
		WaitForSingleObject(this->accept_thread, INFINITE);
		CloseHandle(this->accept_thread);
		this->accept_thread = nullptr;
	}

	for (size_t i = 0; i < this->worker_threads.size(); ++i)
	{
		WaitForSingleObject(this->worker_threads[i], INFINITE);
		CloseHandle(this->worker_threads[i]);
	}
	this->worker_threads.clear();

	if (this->iocp_handle)
	{
		CloseHandle(this->iocp_handle);
		this->iocp_handle = nullptr;
	}

	WSACleanup();
}

unsigned int __stdcall IOCPEchoServer::worker_thread_proc(LPVOID param)
{
	IOCPEchoServer *server;

	server = static_cast<IOCPEchoServer *>(param);
	while (server->running)
	{
		DWORD transferred;
		ULONG_PTR completion_key;
		OVERLAPPED *overlapped;
		BOOL gqcs_ret;

		gqcs_ret = GetQueuedCompletionStatus(
			server->iocp_handle,
			&transferred,
			&completion_key,
			&overlapped,
			INFINITE
		);

		Session *session = reinterpret_cast<Session *>(completion_key);

		if (!overlapped)
			break;

		if (!gqcs_ret || !transferred)
		{
			server->decrement_io_count(session);
			continue;
		}

		if (overlapped == &session->recv_overlapped)
		{
			EnterCriticalSection(&session->lock);
			size_t used;

			session->recv_buffer.advance_write_index(transferred);
			server->recvpost(session);
			used = session->recv_buffer.get_used_size();
			if (used)
			{
				const char *ptr;
				size_t first;
				size_t second;

				first = session->recv_buffer.get_direct_dequeue_size();
				second = used - first;
				ptr = static_cast<const char *>(session->recv_buffer.get_direct_dequeue_ptr());

				if (!session->send_buffer.enqueue(ptr, first))
					DebugBreak();

				if (second)
				{
					const char *wrap = ptr - (session->recv_buffer.get_capacity() - first);
					if (!session->send_buffer.enqueue(wrap, second))
						DebugBreak();
				}

				session->recv_buffer.advance_read_index(used);

				if (InterlockedCompareExchange(&session->sending, 1, 0) == 0)
					if (!server->sendpost(session))
						InterlockedExchange(&session->sending, 0);
			}
			LeaveCriticalSection(&session->lock);
		}
		else if (overlapped == &session->send_overlapped)
		{
			EnterCriticalSection(&session->lock);
			session->send_buffer.advance_read_index(transferred);
			if (session->send_buffer.get_used_size())
				server->sendpost(session);
			else
			{
				InterlockedExchange(&session->sending, 0);
				if (session->send_buffer.get_used_size() && InterlockedCompareExchange(&session->sending, 1, 0) == 0)
					server->sendpost(session);
			}
			LeaveCriticalSection(&session->lock);
		}
		server->decrement_io_count(session);
	}
	return 0;
}

unsigned int __stdcall IOCPEchoServer::accept_thread_proc(LPVOID param)
{
	IOCPEchoServer *server;

	server = static_cast<IOCPEchoServer *>(param);
	while (server->running)
	{
		SOCKADDR_IN addr;
		int addr_len;
		SOCKET client_sock;

		addr_len = sizeof(addr);

		client_sock = accept(server->listen_sock, reinterpret_cast<SOCKADDR *>(&addr), &addr_len);
		if (client_sock == INVALID_SOCKET)
		{
			if (server->running)
				log_system_error("accept()", WSAGetLastError());
			continue;
		}

		Session *session = new (std::nothrow) Session();
		if (!session)
		{
			Logger::get_instance().log(Logger::LogLevel::Error, "New session create failed. No memory");
			closesocket(client_sock);
			continue;
		}

		session->sock = client_sock;
		std::memset(&session->recv_overlapped, 0, sizeof(OVERLAPPED));
		std::memset(&session->send_overlapped, 0, sizeof(OVERLAPPED));
		session->sid = server->next_sid.fetch_add(1);
		InitializeCriticalSection(&session->lock);

		AcquireSRWLockExclusive(&server->session_map_lock);
		server->session_map.emplace(session->sid, session);
		ReleaseSRWLockExclusive(&server->session_map_lock);

		if (!CreateIoCompletionPort(
			reinterpret_cast<HANDLE>(client_sock),
			server->iocp_handle,
			reinterpret_cast<ULONG_PTR>(session),
			0
		))
		{
			AcquireSRWLockExclusive(&server->session_map_lock);
			server->session_map.erase(session->sid);
			ReleaseSRWLockExclusive(&server->session_map_lock);
			delete session;
			closesocket(client_sock);
			log_system_error("CreateIoCompletionPort()", WSAGetLastError());
			continue;
		}

		if (!server->recvpost(session))
		{
			AcquireSRWLockExclusive(&server->session_map_lock);
			server->session_map.erase(session->sid);
			ReleaseSRWLockExclusive(&server->session_map_lock);
			delete session;
			closesocket(client_sock);
			continue;
		}
	}
	return 0;
}

bool IOCPEchoServer::recvpost(Session *session)
{
	std::size_t first;
	std::size_t second;
	std::size_t total_free;
	WSABUF wsabuf[2];
	int buffer_count;
	DWORD flags;
	DWORD recv_bytes;

	total_free = session->recv_buffer.get_free_size();
	if (!total_free)
	{
		DebugBreak();
		return false;
	}

	first = session->recv_buffer.get_direct_enqueue_size();
	second = total_free - first;

	wsabuf[0].buf = static_cast<char *>(session->recv_buffer.get_direct_enqueue_ptr());
	wsabuf[0].len = static_cast<ULONG>(first);
	buffer_count = 1;

	if (second)
	{
		wsabuf[1].buf = wsabuf[0].buf - (session->recv_buffer.get_capacity() - first);
		wsabuf[1].len = static_cast<ULONG>(second);
		buffer_count = 2;
	}

	flags = 0;
	recv_bytes = 0;
	std::memset(&session->recv_overlapped, 0, sizeof(OVERLAPPED));

	InterlockedIncrement(&session->io_count);
	if (WSARecv(
		session->sock,
		wsabuf,
		buffer_count,
		&recv_bytes,
		&flags,
		&session->recv_overlapped,
		nullptr
	) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			decrement_io_count(session);
			return false;
		}
	}
	return true;
}

bool IOCPEchoServer::sendpost(Session *session)
{
	std::size_t first;
	std::size_t second;
	std::size_t total_used;
	WSABUF wsabuf[2];
	int buffer_count;
	DWORD flags;
	DWORD send_bytes;

	total_used = session->send_buffer.get_used_size();
	if (!total_used)
		return false;

	first = session->send_buffer.get_direct_dequeue_size();
	second = total_used - first;

	wsabuf[0].buf = static_cast<char *>(session->send_buffer.get_direct_dequeue_ptr());
	wsabuf[0].len = static_cast<ULONG>(first);
	buffer_count = 1;

	if (second)
	{
		wsabuf[1].buf = wsabuf[0].buf - (session->send_buffer.get_capacity() - first);
		wsabuf[1].len = static_cast<ULONG>(second);
		buffer_count = 2;
	}

	flags = 0;
	send_bytes = 0;
	std::memset(&session->send_overlapped, 0, sizeof(OVERLAPPED));

	InterlockedIncrement(&session->io_count);
	if (WSASend(
		session->sock,
		wsabuf,
		buffer_count,
		&send_bytes,
		flags,
		&session->send_overlapped,
		nullptr
	) == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			decrement_io_count(session);
			return false;
		}

	}
	return true;
}

void IOCPEchoServer::decrement_io_count(Session *session)
{
	if (InterlockedDecrement(&session->io_count) == 0)
	{
		std::uint32_t sid;

		sid = session->sid;

		AcquireSRWLockExclusive(&this->session_map_lock);
		this->session_map.erase(sid);
		EnterCriticalSection(&session->lock);
		LeaveCriticalSection(&session->lock);
		ReleaseSRWLockExclusive(&this->session_map_lock);

		closesocket(session->sock);
		DeleteCriticalSection(&session->lock);
		delete session;
	}
}

static void log_system_error(const char *func_name, DWORD error_code)
{
	LPSTR buffer;

	buffer = nullptr;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		error_code,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		reinterpret_cast<LPSTR>(&buffer),
		0,
		NULL
	);

	if (buffer)
		Logger::get_instance().log(Logger::LogLevel::Error, "%s: %s", func_name, buffer);
	else
		Logger::get_instance().log(Logger::LogLevel::Error, "%s: Unknown Winsock error (Code: %lu)\n", func_name, error_code);
	LocalFree(buffer);
}