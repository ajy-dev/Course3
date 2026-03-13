#include "Game.hpp"

#include <cstdlib>
#include <thread>
#include <chrono>
#include <algorithm>
#include <conio.h>
#include "Logger.hpp"

Game::Game(Server &server)
	: server(&server)
	, world(6400, 6400)
	, running(false)
{
	using namespace std::chrono_literals;
	frame_interval = std::chrono::round<std::chrono::microseconds>(1s / static_cast<double>(FPS));
	std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

void Game::run(void) noexcept
{
	if (!this->server)
	{
		Logger::get_instance().log(Logger::LogLevel::Error, "[Game] No server set. Exiting run().");
		return;
	}

	this->server->set_game(*this);

	this->running = true;
	this->next_frame_timepoint = Clock::now();

	Logger::get_instance().log(Logger::LogLevel::Info, "[Game] Run loop started.");

	while (this->running)
	{
		this->next_frame_timepoint += this->frame_interval;

		if (_kbhit())
		{
			int ch = _getch();

			if (ch == ('Q' & 0x1F))
			{
				Logger::get_instance().log(Logger::LogLevel::Info, "[Game] Ctrl+Q pressed. Shutting down server...");
				this->running = false;
				break;
			}
		}

		this->server->update();
		update();
		Logger::get_instance().flush();

		std::this_thread::sleep_until(this->next_frame_timepoint);
	}

	Logger::get_instance().log(Logger::LogLevel::Info, "[Game] Run loop ended.");
}

void Game::on_client_connected(std::uint32_t client_id) noexcept
{
	constexpr std::uint8_t START_HP = 100;
	constexpr std::uint8_t START_DIR = MOVE_DIR_LL;

	GameWorld::Hero hero;
	uint16_t area_of_interest[9];

	hero.direction = START_DIR;
	hero.hp = START_HP;
	hero.x = std::rand() % GameWorld::WORLD_WIDTH;;
	hero.y = std::rand() % GameWorld::WORLD_HEIGHT;
	this->world.add_hero(client_id, hero);

	actions[client_id].move_direction = hero.direction;
	actions[client_id].moving = false;

	Logger::get_instance().log(
		Logger::LogLevel::Info,
		"[Game] Player %u connected. Spawn at (%u, %u), dir=%u, hp=%u",
		static_cast<unsigned int>(client_id),
		static_cast<unsigned int>(hero.x),
		static_cast<unsigned int>(hero.y),
		static_cast<unsigned int>(hero.direction),
		static_cast<unsigned int>(hero.hp)
	);

	{
		PacketSCCreateMyCharacter packet(
			client_id,
			hero.direction,
			hero.x,
			hero.y,
			hero.hp
		);
		send_packet(client_id, PACKET_SC_CREATE_MY_CHARACTER, packet);
	}

	this->world.get_adjacent_sections(this->world.get_hero(client_id).section, area_of_interest);

	{
		PacketSCCreateOtherCharacter packet(
			client_id,
			hero.direction,
			hero.x,
			hero.y,
			hero.hp
		);

		for (size_t i = 0; i < 9; i++)
		{
			if (area_of_interest[i] != GameWorld::INVALID_SECTION)
			{
				show_section_to_player(client_id, area_of_interest[i]);
				section_broadcast_packet(PACKET_SC_CREATE_OTHER_CHARACTER, packet, area_of_interest[i], client_id);
			}
		}
	}
}

void Game::on_client_disconnected(std::uint32_t client_id) noexcept
{
	PacketSCDeleteCharacter packet(client_id);
	std::uint16_t area_of_interest[9];

	world.get_adjacent_sections(this->world.get_hero(client_id).section, area_of_interest);

	for (size_t i = 0; i < 9; i++)
	{
		if (area_of_interest[i] == GameWorld::INVALID_SECTION)
			continue;
		section_broadcast_packet(PACKET_SC_DELETE_CHARACTER, packet, area_of_interest[i], client_id);
	}

	world.remove_hero(client_id);
	actions[client_id].moving = false;

	Logger::get_instance().log(Logger::LogLevel::Info, "[Game] Player %u disconnected.", static_cast<unsigned int>(client_id));
}

void Game::on_packet_received(std::uint32_t client_id, const void *data, std::size_t size) noexcept
{
	const PacketHeader *header;
	const void *body;
	std::size_t expected_size;

	if (size < sizeof(PacketHeader))
	{
		Logger::get_instance().log(
			Logger::LogLevel::Warning,
			"[Game] ERROR: Received packet from %u is smaller than header size (%zu bytes).",
			static_cast<unsigned int>(client_id),
			size
		);
		return;
	}

	header = reinterpret_cast<const PacketHeader *>(data);
	if (header->code != 0x89)
	{
		Logger::get_instance().log(
			Logger::LogLevel::Warning,
			"[Game] ERROR: Invalid magic code from %u (Code: 0x%X).",
			static_cast<unsigned int>(client_id),
			header->code
		);
		return;
	}

	expected_size = sizeof(PacketHeader) + header->size;
	if (size != expected_size)
	{
		Logger::get_instance().log(
			Logger::LogLevel::Warning,
			"[Game] ERROR: Size mismatch from %u (Type: %u, Expected: %zu, Actual: %zu).",
			static_cast<unsigned int>(client_id),
			header->type,
			expected_size,
			size
		);
		return;
	}

	body = static_cast<const char *>(data) + sizeof(PacketHeader);
	dispatch_packet(client_id, *header, body);
}

void Game::update(void) noexcept
{
	for (std::uint32_t id = 0; id < GameWorld::MAX_HERO; ++id)
	{
		if (!this->world.is_alive(id))
			continue;
		if (!this->actions[id].moving)
			continue;

		progress_player_movement(id);
	}
	// Remove Dead Player
}

void Game::progress_player_movement(std::uint32_t client_id) noexcept
{
	static constexpr std::int8_t STEP_X_TABLE[8] = { -6, -6, 0, 6, 6, 6, 0, -6 };
	static constexpr std::int8_t STEP_Y_TABLE[8] = { 0, -4, -4, -4, 0, 4, 4, 4 };
	const GameWorld::Hero &hero = this->world.get_hero(client_id);
	GameWorld::MoveResult move_result;

	if (is_blocked_by_world_bound(hero.direction, hero.x, hero.y))
		return;

	move_result = this->world.move_hero(
		client_id,
		std::clamp<int>(hero.x + STEP_X_TABLE[hero.direction], 0, GameWorld::WORLD_WIDTH - 1),
		std::clamp<int>(hero.y + STEP_Y_TABLE[hero.direction], 0, GameWorld::WORLD_HEIGHT - 1)
	);

	if (move_result.section_changed)
		handle_section_change(client_id, move_result);

	Logger::get_instance().log(
		Logger::LogLevel::Debug,
		"[Game] Player %u: UPDATE (Dir: %u, X: %u, Y: %u)",
		static_cast<unsigned int>(client_id),
		static_cast<unsigned int>(hero.direction),
		static_cast<unsigned int>(hero.x),
		static_cast<unsigned int>(hero.y)
	);
}

void Game::dispatch_packet(std::uint32_t client_id, const PacketHeader &header, const void *body) noexcept
{
	switch (header.type)
	{
	case PACKET_CS_MOVE_START:
	{
		const PacketCSMoveStart *packet;

		if (!check_body_size<PacketCSMoveStart>(client_id, header))
			return;
		packet = reinterpret_cast<const PacketCSMoveStart *>(body);
		handle_cs_move_start(client_id, *packet);
		break;
	}
	case PACKET_CS_MOVE_STOP:
	{
		const PacketCSMoveStop *packet;

		if (!check_body_size<PacketCSMoveStop>(client_id, header))
			return;
		packet = reinterpret_cast<const PacketCSMoveStop *>(body);
		handle_cs_move_stop(client_id, *packet);
		break;
	}
	case PACKET_CS_ATTACK_1:
	{
		const PacketCSAttack1 *packet;

		if (!check_body_size<PacketCSAttack1>(client_id, header))
			return;
		packet = reinterpret_cast<const PacketCSAttack1 *>(body);
		handle_cs_attack_1(client_id, *packet);
		break;
	}
	case PACKET_CS_ATTACK_2:
	{
		const PacketCSAttack2 *packet;

		if (!check_body_size<PacketCSAttack2>(client_id, header))
			return;
		packet = reinterpret_cast<const PacketCSAttack2 *>(body);
		handle_cs_attack_2(client_id, *packet);
		break;
	}
	case PACKET_CS_ATTACK_3:
	{
		const PacketCSAttack3 *packet;

		if (!check_body_size<PacketCSAttack3>(client_id, header))
			return;
		packet = reinterpret_cast<const PacketCSAttack3 *>(body);
		handle_cs_attack_3(client_id, *packet);
		break;
	}
	default:
		break;
	}
}

void Game::handle_cs_move_start(std::uint32_t client_id, const PacketCSMoveStart &packet) noexcept
{
	constexpr uint16_t TOLERANCE = 50;

	if (packet.direction > MOVE_DIR_LD)
	{
		Logger::get_instance().log(
			Logger::LogLevel::Warning,
			"[Game] Invalid direction from client %u",
			client_id
		);

		this->server->disconnect(client_id);
		return;
	}

	if (this->world.is_alive(client_id))
	{
		ActionState &state = this->actions.at(client_id);
		const GameWorld::Hero &hero = this->world.get_hero(client_id);
		bool already_moving;
		std::uint16_t final_x;
		std::uint16_t final_y;
		std::uint16_t area_of_interest[9];

		this->world.get_adjacent_sections(hero.section, area_of_interest);

		already_moving = state.moving;
		if (already_moving && state.move_direction == packet.direction)
			return;

		if (already_moving)
		{
			const int dx = packet.x - hero.x;
			const int dy = packet.y - hero.y;
			const std::uint16_t max_error = static_cast<uint16_t>(std::max(std::abs(dx), std::abs(dy)));
			
			final_x = packet.x;
			final_y = packet.y;
			if (max_error > TOLERANCE)
			{
				PacketSCSync packet(
					client_id,
					hero.x,
					hero.y
				);

				for (std::size_t i = 0; i < 9; ++i)
				{
					if (area_of_interest[i] == GameWorld::INVALID_SECTION)
						continue;

					section_broadcast_packet(PACKET_SC_SYNC, packet, area_of_interest[i]);
				}

				final_x = hero.x;
				final_y = hero.y;
				Logger::get_instance().log(
					Logger::LogLevel::Warning,
					"[Game] ERROR: Player %u position mismatch (MOVE_CHANGE). Error: %u > %u. Sending Sync.",
					static_cast<unsigned int>(client_id),
					max_error,
					TOLERANCE
				);
			}
		}
		else
		{
			final_x = hero.x;
			final_y = hero.y;
		}
		state.moving = true;
		state.move_direction = packet.direction;

		this->world.move_hero(client_id, packet.direction);

		PacketSCMoveStart response_packet(
			client_id,
			packet.direction,
			final_x,
			final_y
		);

		for (std::size_t i = 0; i < 9; ++i)
		{
			if (area_of_interest[i] == GameWorld::INVALID_SECTION)
				continue;

			section_broadcast_packet(PACKET_SC_MOVE_START, response_packet, area_of_interest[i]);
		}

		Logger::get_instance().log(
			Logger::LogLevel::Info,
			"[Game] Player %u: MOVE %s (Dir: %u, X: %u, Y: %u)",
			static_cast<unsigned int>(client_id),
			already_moving ? "CHANGE" : "START",
			static_cast<unsigned int>(packet.direction),
			static_cast<unsigned int>(final_x),
			static_cast<unsigned int>(final_y)
		);
	}
}

void Game::handle_cs_move_stop(std::uint32_t client_id, const PacketCSMoveStop &packet) noexcept
{
	constexpr uint16_t TOLERANCE = 50;

	if (packet.direction > MOVE_DIR_LD)
	{
		Logger::get_instance().log(
			Logger::LogLevel::Warning,
			"[Game] Invalid direction from client %u",
			client_id
		);

		this->server->disconnect(client_id);
		return;
	}

	if (this->world.is_alive(client_id))
	{
		ActionState &state = this->actions.at(client_id);
		const GameWorld::Hero &hero = this->world.get_hero(client_id);
		const int dx = packet.x - hero.x;
		const int dy = packet.y - hero.y;
		const std::uint16_t max_error = static_cast<uint16_t>(std::max(std::abs(dx), std::abs(dy)));
		std::uint16_t final_x;
		std::uint16_t final_y;
		std::uint16_t area_of_interest[9];

		if (!state.moving)
			return;

		this->world.get_adjacent_sections(hero.section, area_of_interest);

		if (max_error > TOLERANCE)
		{
			PacketSCSync packet(
				client_id,
				hero.x,
				hero.y
			);

			for (std::size_t i = 0; i < 9; ++i)
			{
				if (area_of_interest[i] == GameWorld::INVALID_SECTION)
					continue;

				section_broadcast_packet(PACKET_SC_SYNC, packet, area_of_interest[i]);
			}

			final_x = hero.x;
			final_y = hero.y;
			Logger::get_instance().log(
				Logger::LogLevel::Warning,
				"[Game] ERROR: Player %u position mismatch (MOVE_CHANGE). Error: %u > %u. Sending Sync.",
				static_cast<unsigned int>(client_id),
				max_error,
				TOLERANCE
			);
		}
		else
		{
			final_x = packet.x;
			final_y = packet.y;
		}

		state.moving = false;

		this->world.move_hero(client_id, packet.direction);

		PacketSCMoveStop response_packet(
			client_id,
			packet.direction,
			final_x,
			final_y
		);

		for (std::size_t i = 0; i < 9; ++i)
		{
			if (area_of_interest[i] == GameWorld::INVALID_SECTION)
				continue;

			section_broadcast_packet(PACKET_SC_MOVE_STOP, response_packet, area_of_interest[i]);
		}

		Logger::get_instance().log(
			Logger::LogLevel::Info,
			"[Game] Player %u: MOVE STOP (Dir: %u, X: %u, Y: %u, Error: %u)",
			static_cast<unsigned int>(client_id),
			static_cast<unsigned int>(packet.direction),
			static_cast<unsigned int>(final_x),
			static_cast<unsigned int>(final_y),
			max_error
		);
	}
}

void Game::handle_cs_attack_1(std::uint32_t client_id, const PacketCSAttack1 &packet) noexcept
{
	constexpr std::uint16_t ATTACK_RANGE_X = 80;
	constexpr std::uint16_t ATTACK_RANGE_Y = 10;
	constexpr std::uint8_t ATTACK_DAMAGE = 1;

	if (this->world.is_alive(client_id))
	{
		const GameWorld::Hero &attacker = this->world.get_hero(client_id);
		std::uint16_t area_of_interest[9];
		std::uint16_t x1;
		std::uint16_t x2;
		std::uint16_t y1;
		std::uint16_t y2;

		y1 = attacker.y <= ATTACK_RANGE_Y / 2 ? 0 : attacker.y - ATTACK_RANGE_Y / 2;
		y2 = attacker.y + ATTACK_RANGE_Y / 2;

		if (attacker.direction == MOVE_DIR_LL)
		{
			x1 = attacker.x <= ATTACK_RANGE_X ? 0 : attacker.x - ATTACK_RANGE_X;
			x2 = attacker.x;
		}
		else if (attacker.direction == MOVE_DIR_RR)
		{
			x1 = attacker.x;
			x2 = attacker.x + ATTACK_RANGE_X;
		}
		else
		{
			x1 = attacker.x;
			x2 = attacker.x;
		}

		this->world.get_adjacent_sections(attacker.section, area_of_interest);

		PacketSCAttack1 attack_packet(
			client_id,
			attacker.direction,
			attacker.x,
			attacker.y
		);

		for (size_t i = 0; i < 9; ++i)
			if (area_of_interest[i] != GameWorld::INVALID_SECTION)
				section_broadcast_packet(PACKET_SC_ATTACK_1, attack_packet, area_of_interest[i]);

		Logger::get_instance().log(
			Logger::LogLevel::Info,
			"[Game] Player %u attacks 1. Box: (X1:%u, Y1:%u, X2:%u, Y2:%u)",
			client_id,
			x1,
			y1,
			x2,
			y2
		);

		for (std::uint32_t id = 0; id < GameWorld::MAX_HERO; ++id)
		{
			if (!this->world.is_alive(id) || client_id == id)
				continue;
			else
			{
				const GameWorld::Hero &target = this->world.get_hero(id);
				
				if (target.x >= x1 && target.x <= x2 && target.y >= y1 && target.y <= y2)
				{
					this->world.apply_damage(id, ATTACK_DAMAGE);
					this->world.get_adjacent_sections(target.section, area_of_interest);
					
					PacketSCDamage damage_packet(
						client_id,
						id,
						target.hp
					);

					for (size_t i = 0; i < 9; ++i)
						if (area_of_interest[i] != GameWorld::INVALID_SECTION)
							section_broadcast_packet(PACKET_SC_DAMAGE, damage_packet, area_of_interest[i]);

					if (!target.hp)
						server->disconnect(id);
				}
			}
		}
	}
}

void Game::handle_cs_attack_2(std::uint32_t client_id, const PacketCSAttack2 &packet) noexcept
{
	constexpr std::uint16_t ATTACK_RANGE_X = 90;
	constexpr std::uint16_t ATTACK_RANGE_Y = 10;
	constexpr std::uint8_t ATTACK_DAMAGE = 2;

	if (this->world.is_alive(client_id))
	{
		const GameWorld::Hero &attacker = this->world.get_hero(client_id);
		std::uint16_t area_of_interest[9];
		std::uint16_t x1;
		std::uint16_t x2;
		std::uint16_t y1;
		std::uint16_t y2;

		y1 = attacker.y <= ATTACK_RANGE_Y / 2 ? 0 : attacker.y - ATTACK_RANGE_Y / 2;
		y2 = attacker.y + ATTACK_RANGE_Y / 2;

		if (attacker.direction == MOVE_DIR_LL)
		{
			x1 = attacker.x <= ATTACK_RANGE_X ? 0 : attacker.x - ATTACK_RANGE_X;
			x2 = attacker.x;
		}
		else if (attacker.direction == MOVE_DIR_RR)
		{
			x1 = attacker.x;
			x2 = attacker.x + ATTACK_RANGE_X;
		}
		else
		{
			x1 = attacker.x;
			x2 = attacker.x;
		}

		this->world.get_adjacent_sections(attacker.section, area_of_interest);

		PacketSCAttack2 attack_packet(
			client_id,
			attacker.direction,
			attacker.x,
			attacker.y
		);

		for (size_t i = 0; i < 9; ++i)
			if (area_of_interest[i] != GameWorld::INVALID_SECTION)
				section_broadcast_packet(PACKET_SC_ATTACK_2, attack_packet, area_of_interest[i]);

		Logger::get_instance().log(
			Logger::LogLevel::Info,
			"[Game] Player %u attacks 2. Box: (X1:%u, Y1:%u, X2:%u, Y2:%u)",
			client_id,
			x1,
			y1,
			x2,
			y2
		);

		for (std::uint32_t id = 0; id < GameWorld::MAX_HERO; ++id)
		{
			if (!this->world.is_alive(id) || client_id == id)
				continue;
			else
			{
				const GameWorld::Hero &target = this->world.get_hero(id);

				if (target.x >= x1 && target.x <= x2 && target.y >= y1 && target.y <= y2)
				{
					this->world.apply_damage(id, ATTACK_DAMAGE);
					this->world.get_adjacent_sections(target.section, area_of_interest);

					PacketSCDamage damage_packet(
						client_id,
						id,
						target.hp
					);

					for (size_t i = 0; i < 9; ++i)
						if (area_of_interest[i] != GameWorld::INVALID_SECTION)
							section_broadcast_packet(PACKET_SC_DAMAGE, damage_packet, area_of_interest[i]);

					if (!target.hp)
						server->disconnect(id);
				}
			}
		}
	}
}

void Game::handle_cs_attack_3(std::uint32_t client_id, const PacketCSAttack3 &packet) noexcept
{
	constexpr std::uint16_t ATTACK_RANGE_X = 100;
	constexpr std::uint16_t ATTACK_RANGE_Y = 20;
	constexpr std::uint8_t ATTACK_DAMAGE = 3;

	if (this->world.is_alive(client_id))
	{
		const GameWorld::Hero &attacker = this->world.get_hero(client_id);
		std::uint16_t area_of_interest[9];
		std::uint16_t x1;
		std::uint16_t x2;
		std::uint16_t y1;
		std::uint16_t y2;

		y1 = attacker.y <= ATTACK_RANGE_Y / 2 ? 0 : attacker.y - ATTACK_RANGE_Y / 2;
		y2 = attacker.y + ATTACK_RANGE_Y / 2;

		if (attacker.direction == MOVE_DIR_LL)
		{
			x1 = attacker.x <= ATTACK_RANGE_X ? 0 : attacker.x - ATTACK_RANGE_X;
			x2 = attacker.x;
		}
		else if (attacker.direction == MOVE_DIR_RR)
		{
			x1 = attacker.x;
			x2 = attacker.x + ATTACK_RANGE_X;
		}
		else
		{
			x1 = attacker.x;
			x2 = attacker.x;
		}

		this->world.get_adjacent_sections(attacker.section, area_of_interest);

		PacketSCAttack3 attack_packet(
			client_id,
			attacker.direction,
			attacker.x,
			attacker.y
		);

		for (size_t i = 0; i < 9; ++i)
			if (area_of_interest[i] != GameWorld::INVALID_SECTION)
				section_broadcast_packet(PACKET_SC_ATTACK_3, attack_packet, area_of_interest[i]);

		Logger::get_instance().log(
			Logger::LogLevel::Info,
			"[Game] Player %u attacks 3. Box: (X1:%u, Y1:%u, X2:%u, Y2:%u)",
			client_id,
			x1,
			y1,
			x2,
			y2
		);

		for (std::uint32_t id = 0; id < GameWorld::MAX_HERO; ++id)
		{
			if (!this->world.is_alive(id) || client_id == id)
				continue;
			else
			{
				const GameWorld::Hero &target = this->world.get_hero(id);

				if (target.x >= x1 && target.x <= x2 && target.y >= y1 && target.y <= y2)
				{
					this->world.apply_damage(id, ATTACK_DAMAGE);
					this->world.get_adjacent_sections(target.section, area_of_interest);

					PacketSCDamage damage_packet(
						client_id,
						id,
						target.hp
					);

					for (size_t i = 0; i < 9; ++i)
						if (area_of_interest[i] != GameWorld::INVALID_SECTION)
							section_broadcast_packet(PACKET_SC_DAMAGE, damage_packet, area_of_interest[i]);

					if (!target.hp)
						server->disconnect(id);
				}
			}
		}
	}
}

void Game::handle_section_change(std::uint32_t client_id, const GameWorld::MoveResult &move_result) noexcept
{
	std::uint16_t old_aoi[9];
	std::uint16_t new_aoi[9];
	const GameWorld::Hero &hero = this->world.get_hero(client_id);
	PacketSCDeleteCharacter delete_packet(client_id);
	PacketSCCreateOtherCharacter create_packet(client_id, hero.direction, hero.x, hero.y, hero.hp);
	PacketSCMoveStart move_packet(client_id, hero.direction, hero.x, hero.y);
	std::size_t i;
	std::size_t j;

	this->world.get_adjacent_sections(move_result.old_section, old_aoi);
	this->world.get_adjacent_sections(move_result.new_section, new_aoi);
	i = 0;
	j = 0;
	while (i < 9 || j < 9)
	{
		while (i < 9 && old_aoi[i] == GameWorld::INVALID_SECTION)
			++i;
		while (j < 9 && new_aoi[j] == GameWorld::INVALID_SECTION)
			++j;
	
		if (i < 9 && j < 9)
		{
			if (old_aoi[i] == new_aoi[j])
				++i, ++j;
			else if (old_aoi[i] < new_aoi[j])
			{
				section_broadcast_packet(PACKET_SC_DELETE_CHARACTER, delete_packet, old_aoi[i], client_id);
				hide_section_from_player(client_id, old_aoi[i++]);
			}
			else
			{
				section_broadcast_packet(PACKET_SC_CREATE_OTHER_CHARACTER, create_packet, new_aoi[j], client_id);
				if (this->actions[client_id].moving)
					section_broadcast_packet(PACKET_SC_MOVE_START, move_packet, new_aoi[j], client_id);
				show_section_to_player(client_id, new_aoi[j++]);
			}
		}
		else if (i < 9)
		{
			section_broadcast_packet(PACKET_SC_DELETE_CHARACTER, delete_packet, old_aoi[i], client_id);
			hide_section_from_player(client_id, old_aoi[i++]);
		}
		else if (j < 9)
		{
			section_broadcast_packet(PACKET_SC_CREATE_OTHER_CHARACTER, create_packet, new_aoi[j], client_id);
			if (this->actions[client_id].moving)
				section_broadcast_packet(PACKET_SC_MOVE_START, move_packet, new_aoi[j], client_id);
			show_section_to_player(client_id, new_aoi[j++]);
		}
	}
}

void Game::show_section_to_player(std::uint32_t client_id, std::uint16_t section) noexcept
{
	if (section == GameWorld::INVALID_SECTION)
		return;

	const std::vector<std::uint32_t> &section_players = this->world.get_section_players(section);

	for (std::size_t i = 0; i < section_players.size(); ++i)
	{
		std::uint32_t other_id;

		other_id = section_players[i];

		if (other_id == client_id)
			continue;

		const GameWorld::Hero &other = this->world.get_hero(other_id);

		{
			PacketSCCreateOtherCharacter packet(
				other_id,
				other.direction,
				other.x,
				other.y,
				other.hp
			);

			send_packet(client_id, PACKET_SC_CREATE_OTHER_CHARACTER, packet);
		}

		if (this->actions[other_id].moving)
		{
			PacketSCMoveStart packet(
				other_id,
				other.direction,
				other.x,
				other.y
			);

			send_packet(client_id, PACKET_SC_MOVE_START, packet);
		}
	}
}

void Game::hide_section_from_player(std::uint32_t client_id, std::uint16_t section) noexcept
{
	if (section == GameWorld::INVALID_SECTION)
		return;

	const std::vector<std::uint32_t> &section_players = this->world.get_section_players(section);

	for (std::size_t i = 0; i < section_players.size(); ++i)
	{
		std::uint32_t other_id;

		other_id = section_players[i];

		if (other_id == client_id)
			continue;

		{
			PacketSCDeleteCharacter packet(
				other_id
			);

			send_packet(client_id, PACKET_SC_DELETE_CHARACTER, packet);
		}
	}
}

inline bool Game::is_direction_up(std::uint8_t direction) noexcept
{
	return direction == MOVE_DIR_UU || direction == MOVE_DIR_LU || direction == MOVE_DIR_RU;
}

inline bool Game::is_direction_down(std::uint8_t direction) noexcept
{
	return direction == MOVE_DIR_DD || direction == MOVE_DIR_LD || direction == MOVE_DIR_RD;
}

inline bool Game::is_direction_left(std::uint8_t direction) noexcept
{
	return direction == MOVE_DIR_LL || direction == MOVE_DIR_LU || direction == MOVE_DIR_LD;
}

inline bool Game::is_direction_right(std::uint8_t direction) noexcept
{
	return direction == MOVE_DIR_RR || direction == MOVE_DIR_RU || direction == MOVE_DIR_RD;
}
	
inline bool Game::is_blocked_by_world_bound(std::uint8_t direction, std::uint16_t current_x, std::uint16_t current_y) noexcept
{
	constexpr std::uint16_t WORLD_WEST_END = 0;
	constexpr std::uint16_t WORLD_EAST_END = GameWorld::WORLD_WIDTH - 1;
	constexpr std::uint16_t WORLD_NORTH_END = 0;
	constexpr std::uint16_t WORLD_SOUTH_END = GameWorld::WORLD_HEIGHT - 1;

	if (current_x == WORLD_WEST_END && is_direction_left(direction))
		return true;
	else if (current_x == WORLD_EAST_END && is_direction_right(direction))
		return true;
	else if (current_y == WORLD_NORTH_END && is_direction_up(direction))
		return true;
	else if (current_y == WORLD_SOUTH_END && is_direction_down(direction))
		return true;

	return false;
}