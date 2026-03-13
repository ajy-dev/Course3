#include "SelectServer.hpp"

#define NOMINMAX
#include <Windows.h>
#include <WS2tcpip.h>

#include "Game.hpp"
#include "Packet.hpp"
#include "Logger.hpp"

#include <exception>	// std::bad_alloc
#include <cstdlib>
#include <cassert>
#include <cstring>

static void log_winsock_error(const char *func_name);

SelectServer::SelectServer(std::uint16_t port)
	: port(port)
	, next_sid(nullptr)
{
	try
	{
		this->init_pool();
	}
	catch (const std::exception& exception)
	{
		Logger::get_instance().log(Logger::LogLevel::Fatal, "[SelectServer] init_pool() failed: %s", exception.what());
		Logger::get_instance().flush();
		std::abort();
	}
	catch (...)
	{
		Logger::get_instance().log(Logger::LogLevel::Fatal, "[SelectServer] init_pool() failed: unknown exception");
		Logger::get_instance().flush();
		std::abort();
	}

	if (!this->init_network())
	{
		Logger::get_instance().log(Logger::LogLevel::Fatal, "[SelectServer] Network setup failed");
		Logger::get_instance().flush();
		std::abort();
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "[SelectServer] Initialized");
	Logger::get_instance().flush();
}

SelectServer::~SelectServer(void) noexcept
{
	for (std::uint32_t sid = 0; sid < MAX_SESSION; sid++)
	{
		SOCKET sock;

		sock = this->clients[sid].sock;
		if (sock != INVALID_SOCKET)
		{
			closesocket(sock);
			this->clients[sid].sock = INVALID_SOCKET;
		}
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "[SelectServer] All client sockets closed.");

	if (this->listen_sock != INVALID_SOCKET)
	{
		closesocket(this->listen_sock);
		this->listen_sock = INVALID_SOCKET;
		Logger::get_instance().log(Logger::LogLevel::Info, "[SelectServer] Listen socket closed.");
	}

	WSACleanup();
	Logger::get_instance().log(Logger::LogLevel::Info, "[SelectServer] Winsock cleaned up.");
}

void SelectServer::set_game(Game &game) noexcept
{
	this->game = &game;
}

void SelectServer::update(void) noexcept
{
	if (this->listen_sock == INVALID_SOCKET)
		return;

	this->current_time = Clock::now();
	poll_listen_socket();
	poll_clients();
	check_idle_timeout();
	clean_dead_client();
}

void SelectServer::send_to(std::uint32_t sid, const void *data, std::size_t size) noexcept
{
	if (data && size && sid < MAX_SESSION)
	{
		ClientSession &session = this->clients[sid];

		if (!session.alive || session.sock == INVALID_SOCKET)
			return;

		if (!session.send_buf.enqueue(data, size))
		{
			session.alive = false;
			Logger::get_instance().log(Logger::LogLevel::Warning, "[SelectServer] Send_to enqueue failed, kill client %u.", static_cast<unsigned int>(sid));
		}
	}
}

void SelectServer::broadcast(const void *data, std::size_t size, std::uint32_t except) noexcept
{
	if (!data || !size)
		return;

	for (std::uint32_t sid = 0; sid < MAX_SESSION; sid++)
	{
		ClientSession &session = this->clients[sid];

		if (!session.alive || session.sock == INVALID_SOCKET || sid == except)
			continue;

		if (!session.send_buf.enqueue(data, size))
		{
			session.alive = false;
			Logger::get_instance().log(Logger::LogLevel::Warning, "[SelectServer] Broadcast enqueue failed, kill client %u.", static_cast<unsigned int>(sid));
		}
	}
}

void SelectServer::disconnect(std::uint32_t sid) noexcept
{
	if (sid < MAX_SESSION)
		this->clients[sid].alive = false;
}

void SelectServer::init_pool(void)
{
	for (std::uint32_t i = 0; i <= MAX_SESSION; i++)
		this->free_sids[i] = MAX_SESSION - i;
	Logger::get_instance().log(Logger::LogLevel::Info, "[SelectServer] Sid free-list initialized");

	this->next_sid = &this->free_sids[MAX_SESSION];
	Logger::get_instance().log(
		Logger::LogLevel::Info,
		"[SelectServer] next_sid is set to %u",
		static_cast<unsigned int>(*this->next_sid)
	);

	this->socket_to_sid_table.clear();
	this->socket_to_sid_table.reserve(MAX_SESSION);
	Logger::get_instance().log(Logger::LogLevel::Info, "[SelectServer] socket_to_sid_table reserved");
}

bool SelectServer::init_network(void) noexcept
{
	WSADATA wsa;
	u_long mode;
	SOCKADDR_IN addr;

	if (WSAStartup(MAKEWORD(2, 2), &wsa))
	{
		log_winsock_error("WSAStartup()");
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

	this->listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (this->listen_sock == INVALID_SOCKET)
	{
		log_winsock_error("socket()");
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "Listen socket created.");

	mode = true;
	if (ioctlsocket(this->listen_sock, FIONBIO, &mode) == SOCKET_ERROR)
	{
		log_winsock_error("ioctlsocket()");
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "Listen socket set to non-blocking mode.");

	std::memset(&addr, 0, sizeof(SOCKADDR_IN));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(this->port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(this->listen_sock, reinterpret_cast<PSOCKADDR>(&addr), sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
	{
		log_winsock_error("bind()");
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "IP address bound to listen socket.");

	if (listen(this->listen_sock, SOMAXCONN) == SOCKET_ERROR)
	{
		log_winsock_error("listen()");
		return false;
	}
	Logger::get_instance().log(Logger::LogLevel::Info, "Server listening on port %u.", static_cast<unsigned int>(this->port));
	return true;
}

void SelectServer::poll_listen_socket(void) noexcept
{
	FD_SET readfds;
	TIMEVAL time;

	FD_ZERO(&readfds);
	FD_SET(this->listen_sock, &readfds);

	time.tv_sec = 0;
	time.tv_usec = 0;

	if (select(0, &readfds, nullptr, nullptr, &time) != SOCKET_ERROR)
	{
		if (FD_ISSET(this->listen_sock, &readfds))
			accept_new_clients();
	}
	else
		log_winsock_error("select(listen)");
}

void SelectServer::poll_clients(void) noexcept
{
	TIMEVAL time;

	time.tv_sec = 0;
	time.tv_usec = 0;

	for (std::uint32_t i = 0; i < MAX_SESSION; i += FD_SETSIZE)
	{
		FD_SET readfds;
		FD_SET writefds;
		int ret;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		for (uint32_t j = 0; j < FD_SETSIZE; j++)
		{
			SOCKET sock;
			uint32_t sid;
			
			sid = i + j;
			sock = clients[sid].sock;
			if (sock != INVALID_SOCKET)
			{
				FD_SET(sock, &readfds);
				if (clients[sid].send_buf.get_used_size() > 0)
					FD_SET(sock, &writefds);
			}
		}

		if (!readfds.fd_count && !writefds.fd_count)
			continue;

		ret = select(0, &readfds, &writefds, nullptr, &time);

		if (ret == SOCKET_ERROR)
		{
			log_winsock_error("select()");
			return;
		}

		if (ret > 0)
		{
			for (u_int i = 0; i < readfds.fd_count; i++)
			{
				std::unordered_map<SOCKET, std::uint32_t>::iterator it;
				
				it = this->socket_to_sid_table.find(readfds.fd_array[i]);
				assert(it != this->socket_to_sid_table.end() && "[SelectServer] Sid table invariants corrupted");
				handle_receive(it->second);
			}
			for (u_int i = 0; i < writefds.fd_count; i++)
			{
				std::unordered_map<SOCKET, std::uint32_t>::iterator it;
				std::uint32_t sid;

				it = this->socket_to_sid_table.find(writefds.fd_array[i]);
				assert(it != this->socket_to_sid_table.end() && "[SelectServer] Sid table invariants corrupted");
				sid = it->second;
				if (this->clients[sid].alive)
					handle_send(sid);
			}
		}
	}
}

void SelectServer::accept_new_clients(void)
{
	while (42)
	{
		SOCKADDR_IN addr;
		int addr_len;
		SOCKET sock;
		u_long mode;
		char ip_str[16];
		std::uint32_t sid;

		addr_len = sizeof(SOCKADDR_IN);
		sock = accept(this->listen_sock, reinterpret_cast<PSOCKADDR>(&addr), &addr_len);
		if (sock == INVALID_SOCKET)
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
				log_winsock_error("accept()");
			break;
		}

		if (*this->next_sid == MAX_SESSION)
		{
			Logger::get_instance().log(Logger::LogLevel::Warning, "[SelectServer] Connection rejected: Server is full");
			closesocket(sock);
			continue;
		}

		mode = true;
		if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR)
		{
			log_winsock_error("ioctlsocket()");
			closesocket(sock);
			continue;
		}

		if (!inet_ntop(AF_INET, &(addr.sin_addr), ip_str, 16))
			std::strcpy(ip_str, "Error");

		sid = *this->next_sid;

		try
		{
			if (this->socket_to_sid_table.emplace(sock, sid).second == false)
			{
				assert(false && "[SelectServer] socket_to_sid_table socket duplicate detected");
			}
		}
		catch (const std::exception &exception)
		{
			Logger::get_instance().log(
				Logger::LogLevel::Error,
				"[SelectServer] Failed to register client in lookup table: %s",
				exception.what()
			);
			closesocket(sock);
			continue;
		}

		this->next_sid--;

		this->clients[sid].sock = sock;
		this->clients[sid].recv_buf.clear_buffer();
		this->clients[sid].send_buf.clear_buffer();
		std::strcpy(this->clients[sid].ip_address, ip_str);
		this->clients[sid].port = ntohs(addr.sin_port);
		this->clients[sid].last_activity = this->current_time;
		this->clients[sid].alive = true;

		Logger::get_instance().log(
			Logger::LogLevel::Info,
			"[SelectServer] Client %u connected from %s:%d",
			static_cast<unsigned int>(sid),
			this->clients[sid].ip_address,
			this->clients[sid].port
		);

		if (this->game)
			this->game->on_client_connected(sid);
	}
}

void SelectServer::handle_receive(std::uint32_t sid)
{
	ClientSession &session = this->clients[sid];
	std::size_t free_size;
	int recv_bytes;

	free_size = session.recv_buf.get_direct_enqueue_size();
	if (!free_size)
	{
		session.alive = false;
		Logger::get_instance().log(Logger::LogLevel::Warning, "[SelectServer] Recv buffer full, kill client %u.", static_cast<unsigned int>(sid));
		return;
	}

	recv_bytes = recv(
		session.sock,
		reinterpret_cast<char *>(session.recv_buf.get_direct_enqueue_ptr()),
		static_cast<int>(free_size),
		0
	);
	if (!recv_bytes)
	{
		session.alive = false;
		return;
	}
	else if (recv_bytes == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			log_winsock_error("recv()");
			session.alive = false;
		}
		return;
	}
	session.recv_buf.advance_write_index(static_cast<std::size_t>(recv_bytes));
	session.last_activity = this->current_time;

	process_recv_buffer(sid);
}

void SelectServer::process_recv_buffer(std::uint32_t sid) noexcept
{
	ClientSession &session = this->clients[sid];

	while (42)
	{
		std::size_t used_size;
		PacketHeader header;
		std::size_t packet_size;
		std::uint8_t packet_buffer[sizeof(PacketHeader) + 255];

		if (!session.alive)
			break;
		used_size = session.recv_buf.get_used_size();
		if (used_size < sizeof(PacketHeader))
			break;
		if (!session.recv_buf.peek(&header, sizeof(PacketHeader)))
			break;

		packet_size = sizeof(PacketHeader) + header.size;
		if (used_size < packet_size)
			break;
		if (packet_size > sizeof(packet_buffer))
		{
			session.alive = false;
			Logger::get_instance().log(Logger::LogLevel::Warning, "[SelectServer] Invalid packet size from client %u, kill.", static_cast<unsigned int>(sid));
			break;
		}
		session.recv_buf.dequeue(packet_buffer, packet_size);

		if (this->game)
			this->game->on_packet_received(sid, packet_buffer, packet_size);
	}
}

void SelectServer::handle_send(std::uint32_t sid)
{
	ClientSession &session = this->clients[sid];
	int send_bytes;

	send_bytes = send(
		session.sock,
		reinterpret_cast<const char *>(session.send_buf.get_direct_dequeue_ptr()),
		static_cast<int>(session.send_buf.get_direct_dequeue_size()),
		0
	);

	if (send_bytes > 0)
		session.send_buf.advance_read_index(static_cast<std::size_t>(send_bytes));
	else if (!send_bytes)
		session.alive = false;
	else
	{
		if (WSAGetLastError() != WSAEWOULDBLOCK)
		{
			log_winsock_error("send()");
			session.alive = false;
			Logger::get_instance().log(Logger::LogLevel::Warning, "[SelectServer] send() failed, kill client %u.", static_cast<unsigned int>(sid));
		}
	}
}

void SelectServer::check_idle_timeout(void) noexcept
{
	for (std::uint32_t sid = 0; sid < MAX_SESSION; sid++)
	{
		ClientSession &session = this->clients[sid];

		if (!session.alive || session.sock == INVALID_SOCKET)
			continue;

		if (this->current_time - session.last_activity > IDLE_TIMEOUT)
		{
			session.alive = false;
			Logger::get_instance().log(
				Logger::LogLevel::Warning,
				"[SelectServer] Idle timeout, kill client %u.",
				static_cast<unsigned int>(sid)
			);
		}
	}
}

void SelectServer::clean_dead_client(void)
{
	for (std::uint32_t sid = 0; sid < MAX_SESSION; sid++)
	{
		if (this->clients[sid].sock != INVALID_SOCKET && !this->clients[sid].alive)
		{
			SOCKET sock;

			sock = this->clients[sid].sock;

			this->socket_to_sid_table.erase(sock);
			closesocket(sock);
			this->clients[sid].sock = INVALID_SOCKET;

			if (this->game)
				this->game->on_client_disconnected(sid);

			this->clients[sid].recv_buf.clear_buffer();
			this->clients[sid].send_buf.clear_buffer();

			assert(this->next_sid < &this->free_sids[MAX_SESSION] && "[SelectServer] SID Pool Overflow: Double free detected");
			this->next_sid++;
			*this->next_sid = sid;

			Logger::get_instance().log(
				Logger::LogLevel::Info,
				"[SelectServer] Client %u disconnected from %s:%d",
				static_cast<unsigned int>(sid),
				this->clients[sid].ip_address,
				this->clients[sid].port
			);
		}
	}
}

static void log_winsock_error(const char *func_name)
{
	DWORD error;
	LPSTR buffer;

	error = WSAGetLastError();
	buffer = nullptr;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		error,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		reinterpret_cast<LPSTR>(&buffer),
		0,
		NULL
	);

	if (buffer)
		Logger::get_instance().log(Logger::LogLevel::Error, "%s: %s", func_name, buffer);
	else
		Logger::get_instance().log(Logger::LogLevel::Error, "%s: Unknown Winsock error (Code: %lu)\n", func_name, error);
	LocalFree(buffer);
}