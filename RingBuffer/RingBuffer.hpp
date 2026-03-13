#ifndef RINGBUFFER_HPP
# define RINGBUFFER_HPP

# include <cstdint>	// std::uint64_t
# include <cstddef>	// std::size_t, std::byte

class RingBuffer
{
public:
	static constexpr std::size_t DEFAULT_CAPACITY = 1024;

	explicit RingBuffer(std::size_t buffer_capacity = DEFAULT_CAPACITY) noexcept;
	~RingBuffer(void) noexcept;

	RingBuffer(const RingBuffer &other) = delete;
	RingBuffer &operator=(const RingBuffer &other) = delete;
	RingBuffer(RingBuffer &&other) noexcept;
	RingBuffer &operator=(RingBuffer &&other) noexcept;

	std::size_t get_capacity(void) const noexcept;
	std::size_t get_used_size(void) const noexcept;
	std::size_t get_free_size(void) const noexcept;

	bool enqueue(const void *src, std::size_t size) noexcept;
	bool dequeue(void *dst, std::size_t size) noexcept;
	bool peek(void *dst, std::size_t size) const noexcept;
	void clear_buffer(void) noexcept;

	std::size_t get_direct_enqueue_size(void) const noexcept;
	std::size_t get_direct_dequeue_size(void) const noexcept;
	void advance_read_index(std::size_t size) noexcept;
	void advance_write_index(std::size_t size) noexcept;
	void *get_direct_enqueue_ptr(void) noexcept;
	const void *get_direct_enqueue_ptr(void) const noexcept;
	void *get_direct_dequeue_ptr(void) noexcept;
	const void *get_direct_dequeue_ptr(void) const noexcept;
private:
	std::byte *buffer = nullptr;
	std::size_t capacity;
	std::size_t mask;
	std::uint64_t read_index = 0;
	std::uint64_t write_index = 0;
};

#endif