#ifndef GAME_TPP
# define GAME_TPP

# include <cstring>

template <typename PacketBody>
void Game::send_packet(std::uint32_t client_id, std::uint8_t type, const PacketBody &body) noexcept
{
	PacketHeader header(type, sizeof(PacketBody));
	std::uint8_t buffer[sizeof(PacketHeader) + sizeof(PacketBody)];

	if (!this->server)
		return;

	if (sizeof(PacketBody) > 255)
	{
		std::fprintf(
			stderr,
			"[Game] ERROR: Packet body too large(%zu bytes). Max allowed = 255 bytes.\n",
			sizeof(PacketBody)
		);
		return;
	}

	std::memcpy(buffer, &header, sizeof(PacketHeader));
	std::memcpy(buffer + sizeof(PacketHeader), &body, sizeof(PacketBody));

	this->server->send_to(client_id, buffer, sizeof(buffer));
}

template <typename PacketBody>
void Game::section_broadcast_packet(std::uint8_t type, const PacketBody &body, std::uint16_t section, std::uint32_t except_id) noexcept
{
	PacketHeader header(type, sizeof(PacketBody));
	std::uint8_t buffer[sizeof(PacketHeader) + sizeof(PacketBody)];
	const std::vector<std::uint32_t> &players = this->world.get_section_players(section);

	if (!this->server)
		return;

	if (sizeof(PacketBody) > 255)
	{
		std::fprintf(
			stderr,
			"[Game] ERROR: Packet body too large(%zu bytes). Max allowed = 255 bytes.\n",
			sizeof(PacketBody)
		);
		return;
	}

	std::memcpy(buffer, &header, sizeof(PacketHeader));
	std::memcpy(buffer + sizeof(PacketHeader), &body, sizeof(PacketBody));

	for (size_t i = 0; i < players.size(); ++i)
	{
		std::uint32_t id;

		id = players[i];
		if (id == except_id)
			continue;

		this->server->send_to(id, buffer, sizeof(buffer));
	}
}

template <typename PacketBody>
void Game::global_broadcast_packet(std::uint8_t type, const PacketBody &body, std::uint32_t except_id) noexcept
{
	PacketHeader header(type, sizeof(PacketBody));
	std::uint8_t buffer[sizeof(PacketHeader) + sizeof(PacketBody)];

	if (!this->server)
		return;

	if (sizeof(PacketBody) > 255)
	{
		std::fprintf(
			stderr,
			"[Game] ERROR: Packet body too large(%zu bytes). Max allowed = 255 bytes.\n",
			sizeof(PacketBody)
		);
		return;
	}

	std::memcpy(buffer, &header, sizeof(PacketHeader));
	std::memcpy(buffer + sizeof(PacketHeader), &body, sizeof(PacketBody));

	this->server->broadcast(buffer, sizeof(buffer), except_id);
}

template <typename ExpectedPacketBody>
bool Game::check_body_size(std::uint32_t client_id, const PacketHeader &header) noexcept
{
	constexpr std::size_t expected_size = sizeof(ExpectedPacketBody);

	if (header.size != expected_size)
	{
		std::fprintf(
			stderr,
			"[Game] ERROR: Protocol size mismatch for Type %u from %u. Header claimed: %u, Expected: %zu.\n",
			static_cast<unsigned int>(header.type),
			static_cast<unsigned int>(client_id),
			static_cast<unsigned int>(header.size),
			expected_size
		);
		return false;
	}
	return true;
}

#endif