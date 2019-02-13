#pragma once

#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <cstddef>
#include <iostream>
#include <string>

#ifndef _In_
#define _In_
#endif
#ifndef _In_opt_
#define _In_opt_
#endif

// Some basic definitions.
constexpr unsigned long long KiloByte = (1024);
constexpr unsigned long long MegaByte = (1024 * KiloByte);
constexpr auto mostRestrictiveAlignment = alignof(std::max_align_t) >= 4 ? alignof(std::max_align_t) : 4;

class RBTMemoryAllocator
{
public:
	using SizeType = std::size_t;
private:
	template<class T>
	using Function = std::function<T>;

	struct FreeHeader;
	struct ClaimedHeader;

	struct alignas(mostRestrictiveAlignment) ClaimedHeader
	{
		ClaimedHeader* prev, *next;
	};
	struct alignas(mostRestrictiveAlignment) FreeHeader
		: public ClaimedHeader
	{
		FreeHeader* left = nullptr, *right = nullptr, *parent = nullptr; // RB data. Red/black bit is stored in the least significant bit of parent.
		// These addresses should be perfectly aligned to mostRestrictiveAlignment, so at least 2 least significant bits can be used to store data. In our case: most right tells us if the block is free or claimed and the one on the left is a red black bit.
	};
	struct FittingBlockData
	{
		ClaimedHeader* usableMemory = nullptr;
		SizeType remainingMemory = 0;
		SizeType adjustment = 0;
		SizeType usedMemory = 0;
	};

	static_assert(alignof(FreeHeader) == mostRestrictiveAlignment, "Alignments must match.");
	static_assert(alignof(ClaimedHeader) == mostRestrictiveAlignment, "Alignments must match.");
	static_assert(sizeof(ClaimedHeader) < sizeof(FreeHeader), "It should not happen.");
private:
	void* memory;
	FreeHeader* freeMemory;
	ClaimedHeader* trueMemoryBegin, *endOfMemory;
	SizeType totalMemory, usedMemory;
	unsigned int allocations;
	bool isOwningMemory;
private:
	// Red-black tree methods.
	// true for black, false for red.
	static constexpr inline ClaimedHeader* cleanAddress(const ClaimedHeader* ptr)
	{
		// We must clean two least significant bits.
		return (ClaimedHeader*)(((uintptr_t)ptr) & (~3));
	}
	static constexpr inline SizeType calcSize(const ClaimedHeader* ptr)
	{
		return ((uintptr_t)cleanAddress(ptr->next)) - ((uintptr_t)ptr);
	}
	static constexpr inline bool checkRedness(FreeHeader* const ptr)
	{
		return ((uintptr_t)ptr) & 2;
	}
	static constexpr inline bool isFreeHeader(const ClaimedHeader* ptr)
	{
		return ((uintptr_t)ptr) & 1;
	}
	static constexpr inline FreeHeader* setRedness(FreeHeader* const ptr, const bool red)
	{
		return (FreeHeader*)((((uintptr_t)ptr) & ~2) | (((uintptr_t)red) << 1));
	}
	static constexpr inline ClaimedHeader* setIsFreeHeader(const ClaimedHeader* ptr, const bool isFreeHeader)
	{
		return (ClaimedHeader*)((((uintptr_t)ptr) & ~1) | ((uintptr_t)isFreeHeader));
	}
	static constexpr inline bool getRed(FreeHeader* const ptr)
	{
		return ptr && checkRedness(ptr->parent);
	}

	void removeFromRBTree(FreeHeader* block);
	void insertToRBTree(FreeHeader* block); // It also encodes parent to store the red-black bit.
	bool isBlockFitting(FreeHeader * block, const SizeType reqSize, const SizeType alignment, FittingBlockData& outputData) const;
	FreeHeader* findFittingBlock(const SizeType size, const SizeType alignment, FittingBlockData& outputFittingBlock) const;
	unsigned int dbgCheckFreeTreeSanity() const;
	// Error list:
	// 1: left child looped.
	// 2: right child looped.
	// 3: left child's parent is invalid.
	// 4: right child's parent is invalid.
	// 5: two consecutive reds.
	// 6: black node height is not the same in the whole tree.
	// 7: root is red.
	// 8: root's parent is not null.

	unsigned int dbgGetBlackHeight(FreeHeader* const head) const;
public:
	static RBTMemoryAllocator instance;
public:
	explicit RBTMemoryAllocator(_In_opt_ const SizeType memorySize = 8 * MegaByte);
	RBTMemoryAllocator(_In_ void* memoryToUse, _In_ const SizeType memorySize, _In_opt_ const bool isOwning = false); // Ignore the third parameter.
	~RBTMemoryAllocator();

	void* allocate(_In_ const SizeType howMany, _In_opt_ const SizeType alignment = mostRestrictiveAlignment);
	void deallocate(_In_ void* ptr);

	SizeType getUsedMemory() const;
	SizeType getTotalMemory() const;

	unsigned int getAllocationsCount() const;

	bool isPointerInMemoryRange(_In_ const void* ptr) const;

	unsigned int dbgCalcTreeNodesCount() const;
	void dbgCheckSanity() const; // Will throw std::runtime_error if the error exists. It's empty if _DEBUG is not defined.
	void dbgWriteAllBlocks() const;
	bool dbgCheckListIntegrity() const; // Complexivity: 0(n)
};

template<class T>
class StdAllocator
{
private:
public:
	template<class U>
	struct rebind
	{
		using other = StdAllocator<U>;
	};

	using value_type = T;

	using pointer = T * ;
	using const_pointer = const T*;

	using reference = T & ;
	using const_reference = const T&;

	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;

	using propagate_on_container_move_assignment = std::true_type;
	using is_always_equal = std::true_type;
public:
	StdAllocator() = default;
	StdAllocator(const StdAllocator& arg) = default;
	~StdAllocator() = default;

	template<class U>
	StdAllocator(const StdAllocator<U>&) {};

	value_type* allocate(size_type n);
	void deallocate(pointer p, size_type n);

	bool operator==(const StdAllocator& arg) const;
	bool operator!=(const StdAllocator&) const;
};

template<class T>
inline T * StdAllocator<T>::allocate(size_type n)
{
	return (pointer)RBTMemoryAllocator::instance.allocate(sizeof(value_type)*n, alignof(T));
}
template<class T>
inline void StdAllocator<T>::deallocate(pointer p, size_type)
{
	RBTMemoryAllocator::instance.deallocate(static_cast<void*>(p));
}

template<class T>
inline bool StdAllocator<T>::operator==(const StdAllocator &) const
{
	return true;
}

template<class T>
inline bool StdAllocator<T>::operator!=(const StdAllocator &) const
{
	return false;
}

template<class T>
using Vector = std::vector<T, StdAllocator<T>>;

using String = std::basic_string<char, std::char_traits<char>, StdAllocator<char>>;
