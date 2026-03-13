#include "GameWorld.hpp"
#include <cassert>

GameWorld::GameWorld(std::uint16_t width, std::uint16_t height) noexcept
	: width(width)
	, height(height)
{
	this->alive.fill(false);

	for (std::size_t i = 0; i < TOTAL_SECTION; ++i)
		this->sections[i].reserve(MAX_HERO / TOTAL_SECTION * 2);
}

void GameWorld::add_hero(std::uint32_t id, const Hero &hero)
{
	Hero &h = this->heroes[id];

	h = hero;
	h.section = pos_to_section(hero.x, hero.y);
	this->alive[id] = true;
	this->add_to_section(h.section, id);
}

void GameWorld::remove_hero(std::uint32_t id)
{
	if (!this->alive[id])
		return;

	this->remove_from_section(this->heroes[id].section, id);

	this->heroes[id].section = INVALID_SECTION;
	this->alive[id] = false;
}

GameWorld::MoveResult GameWorld::move_hero(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept
{
	Hero &hero = this->heroes[id];
	MoveResult ret;

	ret.old_section = hero.section;
	ret.new_section = pos_to_section(x, y);

	hero.direction = direction;
	hero.x = x;
	hero.y = y;

	if (ret.section_changed = ret.old_section != ret.new_section)
	{
		hero.section = ret.new_section;
		this->remove_from_section(ret.old_section, id);
		this->add_to_section(ret.new_section, id);
	}

	return ret;
}

GameWorld::MoveResult GameWorld::move_hero(std::uint32_t id, std::uint16_t x, std::uint16_t y) noexcept
{
	return this->move_hero(id, this->heroes[id].direction, x, y);
}

void GameWorld::move_hero(std::uint32_t id, uint8_t direction) noexcept
{
	this->heroes[id].direction = direction;
}

void GameWorld::apply_damage(std::uint32_t id, std::uint8_t damage) noexcept
{
	std::uint8_t &hp = this->heroes[id].hp;

	hp = damage >= hp ? 0 : hp - damage;
}

const GameWorld::Hero &GameWorld::get_hero(std::uint32_t id) const noexcept
{
	return this->heroes[id];
}

const std::vector<std::uint32_t> &GameWorld::get_section_players(std::uint16_t section_index) const noexcept
{
	return this->sections[section_index];
}

void GameWorld::get_adjacent_sections(std::uint16_t section, std::uint16_t out_section[9]) const noexcept
{
	const std::uint16_t sx = section % SECTION_X_COUNT;
	const std::uint16_t sy = section / SECTION_X_COUNT;
	std::size_t index;

	index = 0;
	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dx = -1; dx <= 1; ++dx)
		{
			const int x = static_cast<int>(sx) + dx;
			const int y = static_cast<int>(sy) + dy;

			if (x < 0 || y < 0 || x >= SECTION_X_COUNT || y >= SECTION_Y_COUNT)
				out_section[index++] = INVALID_SECTION;
			else
				out_section[index++] = static_cast<std::uint16_t>(y * SECTION_X_COUNT + x);
		}
	}
}

bool GameWorld::is_alive(std::uint32_t id) const noexcept
{
	return this->alive[id];
}

std::uint16_t GameWorld::pos_to_section(std::uint16_t x, std::uint16_t y) noexcept
{
	const std::uint16_t sx = x / SECTION_SIZE;
	const std::uint16_t sy = y / SECTION_SIZE;

	return sy * SECTION_X_COUNT + sx;
}

void GameWorld::add_to_section(std::uint16_t section, std::uint32_t id)
{
	this->sections[section].push_back(id);
}

void GameWorld::remove_from_section(std::uint16_t section, std::uint32_t id)
{
	std::vector<std::uint32_t> &vec = this->sections[section];
	const std::size_t vec_size = vec.size();

	for (std::size_t i = 0; i < vec_size; ++i)
	{
		if (vec[i] == id)
		{
			vec[i] = vec.back();
			vec.pop_back();
			return;
		}
	}

	assert(false && "[GameWorld] remove_from_section: id not found");
}