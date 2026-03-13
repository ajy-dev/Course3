#ifndef GAMEWORLD_HPP
# define GAMEWORLD_HPP

# include <cstdint>
# include <vector>
# include <array>
# include <limits>

class GameWorld
{
public:
	static constexpr std::uint16_t WORLD_WIDTH = 6400;
	static constexpr std::uint16_t WORLD_HEIGHT = 6400;

	static constexpr std::uint16_t SECTION_SIZE = 100;
	static constexpr std::uint16_t SECTION_X_COUNT = WORLD_WIDTH / SECTION_SIZE;
	static constexpr std::uint16_t SECTION_Y_COUNT = WORLD_HEIGHT / SECTION_SIZE;
	static constexpr std::uint16_t TOTAL_SECTION = SECTION_X_COUNT * SECTION_Y_COUNT;

	static constexpr std::uint16_t MAX_HERO = 10240;

	static constexpr std::uint16_t INVALID_SECTION = std::numeric_limits<std::uint16_t>::max();

	struct Hero
	{
		std::uint8_t direction = 0;
		std::uint16_t x = 0;
		std::uint16_t y = 0;
		std::uint8_t hp = 0;

		std::uint16_t section = INVALID_SECTION;
	};

	struct MoveResult
	{
		bool section_changed = false;
		std::uint16_t old_section = INVALID_SECTION;
		std::uint16_t new_section = INVALID_SECTION;
	};

	GameWorld(std::uint16_t width = WORLD_WIDTH, std::uint16_t height = WORLD_HEIGHT) noexcept;
	~GameWorld(void) = default;

	GameWorld(const GameWorld &other) = delete;
	GameWorld &operator=(const GameWorld &other) = delete;
	GameWorld(GameWorld &&other) = delete;
	GameWorld &operator=(GameWorld &&other) = delete;

	void add_hero(std::uint32_t id, const Hero &hero);
	void remove_hero(std::uint32_t id);
	MoveResult move_hero(std::uint32_t id, std::uint8_t direction, std::uint16_t x, std::uint16_t y) noexcept;
	MoveResult move_hero(std::uint32_t id, std::uint16_t x, std::uint16_t y) noexcept;
	void move_hero(std::uint32_t id, uint8_t direction) noexcept;
	void apply_damage(std::uint32_t id, std::uint8_t damage) noexcept;

	const Hero &get_hero(std::uint32_t id) const noexcept;
	const std::vector<std::uint32_t> &get_section_players(std::uint16_t section_index) const noexcept;
	void get_adjacent_sections(std::uint16_t section, std::uint16_t out_section[9]) const noexcept;
	[[nodiscard]] bool is_alive(std::uint32_t id) const noexcept;

private:
	std::uint16_t width;
	std::uint16_t height;
	std::array<Hero, MAX_HERO> heroes;
	std::array<bool, MAX_HERO> alive;
	std::array<std::vector<std::uint32_t>, TOTAL_SECTION> sections;

	static std::uint16_t pos_to_section(std::uint16_t x, std::uint16_t y) noexcept;

	void add_to_section(std::uint16_t section, std::uint32_t id);
	void remove_from_section(std::uint16_t section, std::uint32_t id);
};

#endif