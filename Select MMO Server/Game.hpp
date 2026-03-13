#ifndef GAME_HPP
# define GAME_HPP

# include <cstdint>
# include <cstddef>
# include <chrono>
# include <type_traits>
# include <array>

# include "Packet.hpp"
# include "GameWorld.hpp"
# include "SelectServer.hpp"

class Game
{
private:
	using Server = SelectServer;
	using Clock = std::conditional<
		std::chrono::high_resolution_clock::is_steady,
		std::chrono::high_resolution_clock,
		std::chrono::steady_clock
	>::type;
	using TimePoint = Clock::time_point;

private:
	Server *server = nullptr;
	GameWorld world;
	bool running = false;

	static constexpr unsigned int FPS = 25;
	std::chrono::microseconds frame_interval;
	TimePoint next_frame_timepoint;

	struct ActionState
	{
		std::uint8_t move_direction = MOVE_DIR_LL;
		bool moving = false;
	};
	std::array<ActionState, GameWorld::MAX_HERO> actions;
public:
	explicit Game(Server &server);
	~Game(void) = default;
	Game(const Game &other) = delete;
	Game &operator=(const Game &other) = delete;
	Game(Game &&other) = delete;
	Game &operator=(Game &&other) = delete;

	void run(void) noexcept;

	void on_client_connected(std::uint32_t client_id) noexcept;
	void on_client_disconnected(std::uint32_t client_id) noexcept;
	void on_packet_received(std::uint32_t client_id, const void *data, std::size_t size) noexcept;

private:
	void update(void) noexcept;
	void progress_player_movement(std::uint32_t client_id) noexcept;
	void dispatch_packet(std::uint32_t client_id, const PacketHeader &header, const void *body) noexcept;

	void handle_cs_move_start(std::uint32_t client_id, const PacketCSMoveStart &packet) noexcept;
	void handle_cs_move_stop(std::uint32_t client_id, const PacketCSMoveStop &packet) noexcept;
	void handle_cs_attack_1(std::uint32_t client_id, const PacketCSAttack1 &packet) noexcept;
	void handle_cs_attack_2(std::uint32_t client_id, const PacketCSAttack2 &packet) noexcept;
	void handle_cs_attack_3(std::uint32_t client_id, const PacketCSAttack3 &packet) noexcept;

	void handle_section_change(std::uint32_t client_id, const GameWorld::MoveResult &move_result) noexcept;
	void show_section_to_player(std::uint32_t client_id, std::uint16_t section) noexcept;
	void hide_section_from_player(std::uint32_t client_id, std::uint16_t section) noexcept;

	static inline bool is_direction_up(std::uint8_t direction) noexcept;
	static inline bool is_direction_down(std::uint8_t direction) noexcept;
	static inline bool is_direction_left(std::uint8_t direction) noexcept;
	static inline bool is_direction_right(std::uint8_t direction) noexcept;
	static inline bool is_blocked_by_world_bound(std::uint8_t direction, std::uint16_t current_x, std::uint16_t current_y) noexcept;

	template <typename PacketBody>
	void send_packet(std::uint32_t client_id, std::uint8_t type, const PacketBody &body) noexcept;
	template <typename PacketBody>
	void section_broadcast_packet(std::uint8_t type, const PacketBody &body, std::uint16_t section, std::uint32_t except_id = -1) noexcept;
	template <typename PacketBody>
	void global_broadcast_packet(std::uint8_t type, const PacketBody &body, std::uint32_t except_id = -1) noexcept;
	template <typename ExpectedPacketBody>
	bool check_body_size(std::uint32_t client_id, const PacketHeader &header) noexcept;
};

# include "Game.tpp"

#endif