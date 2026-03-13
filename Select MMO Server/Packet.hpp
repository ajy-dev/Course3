#ifndef PACKET_HPP
# define PACKET_HPP

# include <cstdint>
# include <cstddef>

# pragma pack(push, 1)
struct PacketHeader
{
	std::uint8_t code;	// 0x89 Fixed
	std::uint8_t size;
	std::uint8_t type;
	PacketHeader() = default;
	PacketHeader(std::uint8_t type, std::uint8_t body_size) noexcept;
};

struct PacketSCCreateMyCharacter
{
	std::uint32_t id;
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	std::uint8_t hp;
	PacketSCCreateMyCharacter() = default;
	PacketSCCreateMyCharacter(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y, std::uint8_t hp) noexcept;
};

struct PacketSCCreateOtherCharacter
{
	std::uint32_t id;
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	std::uint8_t hp;
	PacketSCCreateOtherCharacter() = default;
	PacketSCCreateOtherCharacter(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y, std::uint8_t hp) noexcept;
};

struct PacketSCDeleteCharacter
{
	std::uint32_t id;
	PacketSCDeleteCharacter() = default;
	PacketSCDeleteCharacter(std::uint32_t id) noexcept;
};

struct PacketCSMoveStart
{
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketCSMoveStart() = default;
	PacketCSMoveStart(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketSCMoveStart
{
	std::uint32_t id;
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketSCMoveStart() = default;
	PacketSCMoveStart(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketCSMoveStop
{
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketCSMoveStop() = default;
	PacketCSMoveStop(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketSCMoveStop
{
	std::uint32_t id;
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketSCMoveStop() = default;
	PacketSCMoveStop(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketCSAttack1
{
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketCSAttack1() = default;
	PacketCSAttack1(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketSCAttack1
{
	std::uint32_t id;
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketSCAttack1() = default;
	PacketSCAttack1(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketCSAttack2
{
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketCSAttack2() = default;
	PacketCSAttack2(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketSCAttack2
{
	std::uint32_t id;
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketSCAttack2() = default;
	PacketSCAttack2(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketCSAttack3
{
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketCSAttack3() = default;
	PacketCSAttack3(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketSCAttack3
{
	std::uint32_t id;
	std::uint8_t direction;
	std::uint16_t x;
	std::uint16_t y;
	PacketSCAttack3() = default;
	PacketSCAttack3(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketSCDamage
{
	std::uint32_t source_id;
	std::uint32_t target_id;
	std::uint8_t target_hp;
	PacketSCDamage() = default;
	PacketSCDamage(std::uint32_t source_id, std::uint32_t target_id, std::uint8_t target_hp) noexcept;
};

struct PacketSCSync
{
	std::uint32_t id;
	std::uint16_t x;
	std::uint16_t y;
	PacketSCSync() = default;
	PacketSCSync(std::uint32_t id, std::uint16_t x, std::uint16_t y) noexcept;
};

struct PacketCSEcho
{
	std::uint32_t time;
	PacketCSEcho() = default;
	PacketCSEcho(std::uint32_t time) noexcept;
};

struct PacketSCEcho
{
	std::uint32_t time;
	PacketSCEcho() = default;
	PacketSCEcho(std::uint32_t time) noexcept;
};

# pragma pack(pop)

enum : std::uint8_t
{
	PACKET_SC_CREATE_MY_CHARACTER = 0,
	PACKET_SC_CREATE_OTHER_CHARACTER = 1,
	PACKET_SC_DELETE_CHARACTER = 2,
	PACKET_CS_MOVE_START = 10,
	PACKET_SC_MOVE_START = 11,
	PACKET_CS_MOVE_STOP = 12,
	PACKET_SC_MOVE_STOP = 13,
	PACKET_CS_ATTACK_1 = 20,
	PACKET_SC_ATTACK_1 = 21,
	PACKET_CS_ATTACK_2 = 22,
	PACKET_SC_ATTACK_2 = 23,
	PACKET_CS_ATTACK_3 = 24,
	PACKET_SC_ATTACK_3 = 25,
	PACKET_SC_DAMAGE = 30,
	PACKET_SC_SYNC = 251,
	PACKET_CS_ECHO = 252,
	PACKET_SC_ECHO = 253
};

enum : std::uint8_t
{
	MOVE_DIR_LL = 0,
	MOVE_DIR_LU = 1,
	MOVE_DIR_UU = 2,
	MOVE_DIR_RU = 3,
	MOVE_DIR_RR = 4,
	MOVE_DIR_RD = 5,
	MOVE_DIR_DD = 6,
	MOVE_DIR_LD = 7
};

#endif