#ifndef SELECTSERVER_HPP
# define SELECTSERVER_HPP

# define NOMINMAX
# include <WinSock2.h>
# pragma comment(lib, "ws2_32.lib")

# include <cstdint>
# include <cstddef>
# include <unordered_map>
# include <chrono>
# include <type_traits>

# include "RingBuffer.hpp"
# include "Packet.hpp"

class Game;

class SelectServer
{
public:
	explicit SelectServer(std::uint16_t port);
	~SelectServer(void) noexcept;

	SelectServer(const SelectServer &other) = delete;
	SelectServer &operator=(const SelectServer &other) = delete;

	SelectServer(SelectServer &&other) = delete;
	SelectServer &operator=(SelectServer &&other) = delete;

	void set_game(Game &game) noexcept;
	void update(void) noexcept;
	void send_to(std::uint32_t sid, const void *data, std::size_t size) noexcept;
	void broadcast(const void *data, std::size_t size, std::uint32_t except = -1) noexcept;
	void disconnect(std::uint32_t client_id) noexcept;
private:
	using Clock = std::conditional<
		std::chrono::high_resolution_clock::is_steady,
		std::chrono::high_resolution_clock,
		std::chrono::steady_clock
	>::type;
	using TimePoint = Clock::time_point;

	struct ClientSession
	{
		SOCKET sock = INVALID_SOCKET;
		RingBuffer recv_buf = RingBuffer(160000);
		RingBuffer send_buf = RingBuffer(160000);
		char ip_address[16];
		std::uint16_t port;
		TimePoint last_activity;
		bool alive;
	};

	static constexpr std::uint32_t MAX_SESSION = FD_SETSIZE * 160; // 10240
	static constexpr Clock::duration IDLE_TIMEOUT = std::chrono::duration_cast<Clock::duration>(std::chrono::seconds(30));

	void init_pool(void);
	bool init_network(void) noexcept;
	void poll_listen_socket(void) noexcept;
	void poll_clients(void) noexcept;

	void accept_new_clients(void);
	void handle_receive(std::uint32_t sid);
	void process_recv_buffer(std::uint32_t sid) noexcept;
	void handle_send(std::uint32_t sid);
	void check_idle_timeout(void) noexcept;
	void clean_dead_client(void);

	SOCKET listen_sock = INVALID_SOCKET;
	std::uint16_t port = 0;

	ClientSession clients[MAX_SESSION];
	std::uint32_t free_sids[MAX_SESSION + 1];
	std::uint32_t *next_sid;
	std::unordered_map<SOCKET, std::uint32_t> socket_to_sid_table;
	TimePoint current_time;

	Game *game = nullptr;	// To be removed later
};

#endif