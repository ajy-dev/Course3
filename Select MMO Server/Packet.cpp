#include "Packet.hpp"

PacketHeader::PacketHeader(std::uint8_t type, std::uint8_t body_size) noexcept
	: code(0x89)
	, size(static_cast<std::uint8_t>(body_size))
	, type(type)
{
}

PacketSCCreateMyCharacter::PacketSCCreateMyCharacter(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y, std::uint8_t hp) noexcept
	: id(id)
	, direction(direction)
	, x(x)
	, y(y)
	, hp(hp)
{
}

PacketSCCreateOtherCharacter::PacketSCCreateOtherCharacter(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y, std::uint8_t hp) noexcept
	: id(id)
	, direction(direction)
	, x(x)
	, y(y)
	, hp(hp)
{
}

PacketSCDeleteCharacter::PacketSCDeleteCharacter(std::uint32_t id) noexcept
	: id(id)
{
}

PacketCSMoveStart::PacketCSMoveStart(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: direction(direction)
	, x(x)
	, y(y)
{
}

PacketSCMoveStart::PacketSCMoveStart(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: id(id)
	, direction(direction)
	, x(x)
	, y(y)
{
}

PacketCSMoveStop::PacketCSMoveStop(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: direction(direction)
	, x(x)
	, y(y)
{
}

PacketSCMoveStop::PacketSCMoveStop(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: id(id)
	, direction(direction)
	, x(x)
	, y(y)
{
}

PacketCSAttack1::PacketCSAttack1(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: direction(direction)
	, x(x)
	, y(y)
{
}

PacketSCAttack1::PacketSCAttack1(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: id(id)
	, direction(direction)
	, x(x)
	, y(y)
{
}

PacketCSAttack2::PacketCSAttack2(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: direction(direction)
	, x(x)
	, y(y)
{
}

PacketSCAttack2::PacketSCAttack2(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: id(id)
	, direction(direction)
	, x(x)
	, y(y)
{
}

PacketCSAttack3::PacketCSAttack3(std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: direction(direction)
	, x(x)
	, y(y)
{
}

PacketSCAttack3::PacketSCAttack3(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
	: id(id)
	, direction(direction)
	, x(x)
	, y(y)
{
}

PacketSCDamage::PacketSCDamage(std::uint32_t source_id, std::uint32_t target_id, std::uint8_t target_hp) noexcept
	: source_id(source_id)
	, target_id(target_id)
	, target_hp(target_hp)
{
}

PacketSCSync::PacketSCSync(std::uint32_t id, std::uint16_t x, std::uint16_t y) noexcept
	: id(id)
	, x(x)
	, y(y)
{
}

PacketCSEcho::PacketCSEcho(std::uint32_t time) noexcept
	: time(time)
{
}

PacketSCEcho::PacketSCEcho(std::uint32_t time) noexcept
	: time(time)
{
}