#include "RBTMemoryAllocator.h"

using std::swap;

RBTMemoryAllocator RBTMemoryAllocator::instance;

template<class T, class Y>
inline constexpr T* addPointers(T* const arg, const Y byHowMany)
{
	return (T*)(((uintptr_t)arg) + ((uintptr_t)byHowMany));
}

template<class T, class Y>
inline constexpr T* subPointers(T* const arg, const Y byHowMany)
{
	return (T*)(((uintptr_t)arg) - ((uintptr_t)byHowMany));
}

RBTMemoryAllocator::SizeType RBTMemoryAllocator::getUsedMemory() const
{
	return usedMemory;
}

RBTMemoryAllocator::SizeType RBTMemoryAllocator::getTotalMemory() const
{
	return totalMemory;
}

unsigned int RBTMemoryAllocator::getAllocationsCount() const
{
	return allocations;
}

bool RBTMemoryAllocator::isPointerInMemoryRange(_In_ const void * ptr) const
{
	return (ptr > trueMemoryBegin) && (ptr < endOfMemory);
}

unsigned int RBTMemoryAllocator::dbgCalcTreeNodesCount() const
{
	static Function<void(FreeHeader*, const Function<void(FreeHeader*)>&)> traverseBlock = [](FreeHeader* block, const Function<void(FreeHeader*)>& func)
	{
		if (!block)
		{
			return;
		}

		traverseBlock(block->left, func);
		func(block);
		traverseBlock(block->right, func);
	};

	unsigned int count = 0;

	traverseBlock(freeMemory, [&count](FreeHeader*)
	{
		++count;
	});

	return count;
}

void RBTMemoryAllocator::dbgCheckSanity() const
{
	auto temp = dbgCheckFreeTreeSanity();
	if (temp)
	{
		throw temp;
	}
}

void RBTMemoryAllocator::removeFromRBTree(FreeHeader * block)
{
	if (!block)
	{
		return;
	}

	bool childRed = true, toRemoveRed = checkRedness(block->parent);
	FreeHeader* replacement = nullptr, *replaceParent = nullptr;

	if (block->left && block->right)
	{
		// Two children.
		FreeHeader* succ = block->right;

		while (succ->left)
		{
			succ = succ->left;
		}

		FreeHeader* tempGarbage = nullptr;
		FreeHeader*& toRemoveInParent = cleanAddress(block->parent) ? (((FreeHeader*)cleanAddress(block->parent))->left == block ? ((FreeHeader*)cleanAddress(block->parent))->left : ((FreeHeader*)cleanAddress(block->parent))->right) : tempGarbage;

		if (succ == block->right)
		{
			succ->left = block->left;
			block->left->parent = setRedness(succ, checkRedness(block->left->parent));
			block->left = nullptr;

			const bool tempRed = checkRedness(succ->parent);
			succ->parent = block->parent;
			toRemoveInParent = succ;
			block->parent = setRedness(succ, tempRed);

			block->right = succ->right;
			succ->right = block;
			if (block->right)
			{
				block->right->parent = setRedness(block, checkRedness(block->right->parent));
			}
		}
		else
		{
			FreeHeader*& succInParent = ((FreeHeader*)cleanAddress(succ->parent))->left == succ ? ((FreeHeader*)cleanAddress(succ->parent))->left : ((FreeHeader*)cleanAddress(succ->parent))->right;

			block->left->parent = setRedness(succ, checkRedness(block->left->parent));
			block->right->parent = setRedness(succ, checkRedness(block->right->parent));
			if (succ->right)
			{
				succ->right->parent = setRedness(block, checkRedness(succ->right->parent));
			}

			swap(block->left, succ->left);
			swap(block->right, succ->right);
			swap(block->parent, succ->parent); // Also the reddness is swaped.

			toRemoveInParent = succ;
			succInParent = block;
		}

		if (block == freeMemory)
		{
			freeMemory = succ;
		}

		removeFromRBTree(block);
		return;
	}
	else
		if (block->left || block->right)
		{
			// One child.
			FreeHeader* child = block->left ? block->left : block->right;
			replacement = child;

			if (block == freeMemory)
			{
				// No parent.
				freeMemory = child;
			}
			else
			{
				FreeHeader*& toRemoveInParent = ((FreeHeader*)cleanAddress(block->parent))->left == block ? ((FreeHeader*)cleanAddress(block->parent))->left : ((FreeHeader*)cleanAddress(block->parent))->right;
				toRemoveInParent = child;
			}

			child->parent = setRedness(block->parent, checkRedness(child->parent));
		}
		else
		{
			// An orphan.
			if (block == freeMemory)
			{
				freeMemory = nullptr;
				return;
			}
			else
			{
				FreeHeader*& toRemoveInParent = ((FreeHeader*)cleanAddress(block->parent))->left == block ? ((FreeHeader*)cleanAddress(block->parent))->left : ((FreeHeader*)cleanAddress(block->parent))->right;
				toRemoveInParent = nullptr;
			}
		}

	replaceParent = (FreeHeader*)cleanAddress(block->parent);
	childRed = replacement ? checkRedness(replacement->parent) : false;

	if (childRed || toRemoveRed)
	{
		if (replacement)
		{
			replacement->parent = setRedness(replacement->parent, false);
		}
	}
	else
	{
		// Double black.
		FreeHeader* doubleBlack = replacement, *dbParent = replaceParent, *sibling = doubleBlack, *tempGarbage = nullptr;

		while (doubleBlack != freeMemory)
		{
			sibling = dbParent->left == doubleBlack ? dbParent->right : dbParent->left;

			if (!getRed(sibling))
			{
				if (sibling && (getRed(sibling->left) || getRed(sibling->right)))
				{
					FreeHeader* redNode = getRed(sibling->left) ? sibling->left : sibling->right;
					FreeHeader*& dbParentInParent = cleanAddress(dbParent->parent) ? (((FreeHeader*)cleanAddress(dbParent->parent))->left == dbParent ? ((FreeHeader*)cleanAddress(dbParent->parent))->left : ((FreeHeader*)cleanAddress(dbParent->parent))->right) : tempGarbage;

					// https://www.geeksforgeeks.org/red-black-tree-set-3-delete-2/
					if (dbParent->left == sibling)
					{
						if (sibling->left == redNode)
						{
							// Left left.
							// Mirror of right right.
							dbParent->left = sibling->right;
							if (sibling->right)
							{
								sibling->right->parent = setRedness(dbParent, checkRedness(sibling->right->parent));
							}
							sibling->right = dbParent;
							const bool tempRed = checkRedness(sibling->parent);
							sibling->parent = dbParent->parent;
							dbParent->parent = setRedness(sibling, tempRed);
							dbParentInParent = sibling;
							sibling->left->parent = setRedness(sibling, false);

							if (sibling->parent == nullptr)
							{
								freeMemory = sibling;
							}
						}
						else
						{
							// Left right.
							// First rotation.
							FreeHeader* thing = sibling->right;
							sibling->right = thing->left;
							if (sibling->right)
							{
								sibling->right->parent = setRedness(sibling, checkRedness(sibling->right->parent));
							}
							thing->left = sibling;
							thing->parent = setRedness(dbParent, false);
							(dbParent->left == sibling ? dbParent->left : dbParent->right) = thing;
							sibling->parent = setRedness(thing, false);

							// Second rotation.
							thing->parent = dbParent->parent;
							dbParentInParent = thing;
							dbParent->parent = setRedness(thing, false);
							dbParent->left = thing->right;
							if (dbParent->left)
							{
								dbParent->left->parent = setRedness(dbParent, checkRedness(dbParent->left->parent));
							}
							thing->right = dbParent;

							if (thing->parent == nullptr)
							{
								freeMemory = thing;
							}
						}
					}
					else
					{
						if (sibling->right == redNode || getRed(sibling->right))
						{
							// Right right.
							dbParent->right = sibling->left;
							if (sibling->left)
							{
								sibling->left->parent = setRedness(dbParent, checkRedness(sibling->left->parent));
							}
							sibling->left = dbParent;
							const bool tempRed = checkRedness(sibling->parent);
							sibling->parent = dbParent->parent;
							dbParent->parent = setRedness(sibling, tempRed);
							dbParentInParent = sibling;
							sibling->right->parent = setRedness(sibling, false);

							if (sibling->parent == nullptr)
							{
								freeMemory = sibling;
							}
						}
						else
						{
							// Right left.
							// First rotation.
							FreeHeader* thing = sibling->left;
							sibling->left = thing->right;
							if (sibling->left)
							{
								sibling->left->parent = setRedness(sibling, checkRedness(sibling->left->parent));
							}
							thing->right = sibling;
							thing->parent = setRedness(dbParent, false);
							(dbParent->left == sibling ? dbParent->left : dbParent->right) = thing;
							sibling->parent = setRedness(thing, false);

							// Second rotation.
							thing->parent = dbParent->parent;
							dbParentInParent = thing;
							dbParent->parent = setRedness(thing, false);
							dbParent->right = thing->left;
							if (dbParent->right)
							{
								dbParent->right->parent = setRedness(dbParent, checkRedness(dbParent->right->parent));
							}
							thing->left = dbParent;

							if (thing->parent == nullptr)
							{
								freeMemory = thing;
							}
						}
					}

					// Restructurization done, no two consecutive reds principle restored.
					break;
				}
				else
				{
					// Both are black.
					if (sibling)
					{
						sibling->parent = setRedness(sibling->parent, true);
					}
					if (checkRedness(dbParent->parent))
					{
						// We can color red parent to black, and therefore, stop the recursion.
						dbParent->parent = setRedness(dbParent->parent, false);
						break;
					}
					doubleBlack = dbParent;
					dbParent = (FreeHeader*)cleanAddress(doubleBlack->parent);
				}
			}
			else
			{
				FreeHeader*& dbParentInParent = cleanAddress(dbParent->parent) ? ((FreeHeader*)cleanAddress(dbParent->parent))->left == dbParent ? ((FreeHeader*)cleanAddress(dbParent->parent))->left : ((FreeHeader*)cleanAddress(dbParent->parent))->right : tempGarbage;

				// Sibling is red.
				if (((FreeHeader*)cleanAddress(sibling->parent))->right == sibling)
				{
					// Right rotate.
					dbParent->right = sibling->left;
					if (sibling->left)
					{
						sibling->left->parent = setRedness(dbParent, checkRedness(sibling->left->parent));
					}
					sibling->left = dbParent;
					const bool tempRed = checkRedness(sibling->parent);
					sibling->parent = dbParent->parent;
					dbParent->parent = setRedness(sibling, tempRed);
					dbParentInParent = sibling;
					sibling->right->parent = setRedness(sibling, false);
				}
				else
				{
					// Left rotate.
					dbParent->left = sibling->right;
					if (sibling->right)
					{
						sibling->right->parent = setRedness(dbParent, checkRedness(sibling->right->parent));
					}
					sibling->right = dbParent;
					const bool tempRed = checkRedness(sibling->parent);
					sibling->parent = dbParent->parent;
					dbParent->parent = setRedness(sibling, tempRed);
					dbParentInParent = sibling;
					sibling->left->parent = setRedness(sibling, false);
				}

				if (sibling->parent == nullptr)
				{
					freeMemory = sibling;
				}
			}
		}
	}
}

void RBTMemoryAllocator::insertToRBTree(FreeHeader * block)
{
	if (freeMemory == nullptr)
	{
		freeMemory = block;
		freeMemory->parent = setRedness(nullptr, false);
		return;
	}

	FreeHeader* ptr = freeMemory;
	const SizeType blockSize = calcSize(block);

	while (true)
	{
		if (blockSize < calcSize(ptr))
		{
			if (ptr->left)
			{
				ptr = ptr->left;
			}
			else
			{
				ptr->left = block;
				block->parent = setRedness(ptr, true);
				ptr = block;
				break;
			}
		}
		else
		{
			if (ptr->right)
			{
				ptr = ptr->right;
			}
			else
			{
				ptr->right = block;
				block->parent = setRedness(ptr, true);
				ptr = block;
				break;
			}
		}
	}

	while (true)
	{
		if (ptr == freeMemory)
		{
			ptr->parent = setRedness(ptr->parent, false);
		}
		else
			if (getRed((FreeHeader*)cleanAddress(ptr->parent)))
			{
				// Recolor the tree.
				FreeHeader* parent = (FreeHeader*)cleanAddress(ptr->parent), *grandparent = (FreeHeader*)cleanAddress(parent->parent);

				if (!grandparent)
				{
					break;;
				}

				FreeHeader* uncle = grandparent->left == parent ? grandparent->right : grandparent->left;

				if (getRed(uncle))
				{
					parent->parent = setRedness(parent->parent, false);
					uncle->parent = setRedness(uncle->parent, false);
					grandparent->parent = setRedness(grandparent->parent, true);

					ptr = grandparent;
					continue;
				}
				else
				{
					static auto leftLeft = [this](FreeHeader* ptr, FreeHeader* parent, FreeHeader* grandparent)
					{
						grandparent->left = parent->right;
						if (grandparent->left)
						{
							grandparent->left->parent = setRedness(grandparent, checkRedness(grandparent->left->parent));
						}

						const bool tempRed = checkRedness(parent->parent);
						parent->parent = grandparent->parent;
						if (cleanAddress(parent->parent))
						{
							(((FreeHeader*)cleanAddress(parent->parent))->left == grandparent ? ((FreeHeader*)cleanAddress(parent->parent))->left : ((FreeHeader*)cleanAddress(parent->parent))->right) = parent;
						}
						else
						{
							freeMemory = parent;
						}
						grandparent->parent = setRedness(parent, tempRed);

						parent->right = grandparent;
					};
					static auto rightRight = [this](FreeHeader* ptr, FreeHeader* parent, FreeHeader* grandparent)
					{
						grandparent->right = parent->left;
						if (grandparent->right)
						{
							grandparent->right->parent = setRedness(grandparent, checkRedness(grandparent->right->parent));
						}

						const bool tempRed = checkRedness(parent->parent);
						parent->parent = grandparent->parent;
						if (cleanAddress(parent->parent))
						{
							(((FreeHeader*)cleanAddress(parent->parent))->left == grandparent ? ((FreeHeader*)cleanAddress(parent->parent))->left : ((FreeHeader*)cleanAddress(parent->parent))->right) = parent;
						}
						else
						{
							freeMemory = parent;
						}
						grandparent->parent = setRedness(parent, tempRed);

						parent->left = grandparent;
					};

					if (grandparent && parent == grandparent->left)
					{
						if (ptr == parent->left)
						{
							// Left left.
							leftLeft(ptr, parent, grandparent);
						}
						else
						{
							// Left right.
							parent->right = ptr->left;
							if (parent->right)
							{
								parent->right->parent = setRedness(parent, checkRedness(parent->right->parent));
							}
							ptr->left = parent;
							ptr->parent = setRedness(grandparent, checkRedness(ptr->parent));
							parent->parent = setRedness(ptr, checkRedness(parent->parent));
							(grandparent->left == parent ? grandparent->left : grandparent->right) = ptr;
							leftLeft(parent, ptr, grandparent);
						}
					}
					else
					{
						if (ptr == parent->left)
						{
							// Right left.
							parent->left = ptr->right;
							if (parent->left)
							{
								parent->left->parent = setRedness(parent, checkRedness(parent->left->parent));
							}
							ptr->right = parent;
							ptr->parent = setRedness(grandparent, checkRedness(ptr->parent));
							parent->parent = setRedness(ptr, checkRedness(parent->parent));
							if (grandparent)
							{
								(grandparent->left == parent ? grandparent->left : grandparent->right) = ptr;
							}
							rightRight(parent, ptr, grandparent);
						}
						else
						{
							// Right right.
							rightRight(ptr, parent, grandparent);
						}
					}

					// Restructurization complete.
				}
			}
		break;
	}
}

inline bool RBTMemoryAllocator::isBlockFitting(FreeHeader * block, const SizeType reqSize, const SizeType alignment, FittingBlockData& outputData) const
{
	// Assumption: two free blocks cannot be next to each other.
	if (!block)
	{
		return false;
	}
	void* ptr = addPointers(block, sizeof(ClaimedHeader));
	SizeType space = calcSize(block) - sizeof(ClaimedHeader);
	const SizeType startingSpace = space;
	FittingBlockData& result = outputData;

	const SizeType requiredSize = ((reqSize % mostRestrictiveAlignment) == 0) ? reqSize : reqSize + (mostRestrictiveAlignment - (reqSize % mostRestrictiveAlignment));

	// We don't check if it's possible to attach any remaining memory to existing block. Also we don't check if it's possible to create new free block out of remaining memory.
	// That's because remaining memory, assuming we're unable to allocate new free blocks out of it, is attached to already existing claimed blocks.
	// There is only one case to check - whether prev is nullptr.
	if (block->prev == nullptr)
	{
		if (space >= sizeof(FreeHeader))
		{
			space -= sizeof(FreeHeader);
			ptr = addPointers(ptr, sizeof(FreeHeader));
		}
		else
		{
			return false;
		}
	}

	if (!std::align(alignment, requiredSize, ptr, space))
	{
		return false;
	}

	result.adjustment = ((SizeType)subPointers(ptr, block)) - sizeof(ClaimedHeader);
	result.usableMemory = (ClaimedHeader*)subPointers(ptr, sizeof(ClaimedHeader));
	result.remainingMemory = startingSpace - result.adjustment - requiredSize;
	result.usedMemory = requiredSize;

	return true;
}

RBTMemoryAllocator::FreeHeader * RBTMemoryAllocator::findFittingBlock(const SizeType size, const SizeType alignment, FittingBlockData& outputFittingBlock) const
{
	FreeHeader* searchNode = freeMemory, *chosenBlock = nullptr;
	FittingBlockData isFittingData, &chosenData = outputFittingBlock;
	chosenData.remainingMemory = (~0ULL);

	if (isBlockFitting(freeMemory, size, alignment, isFittingData))
	{
		chosenBlock = freeMemory;
		chosenData = isFittingData;
	}

	while (searchNode && (searchNode->left || searchNode->right))
	{
		if (isBlockFitting(searchNode->left, size, alignment, isFittingData))
		{
			chosenData = isFittingData;
			searchNode = chosenBlock = searchNode->left;
		}
		else
			if (isBlockFitting(searchNode->right, size, alignment, isFittingData))
			{
				chosenData = isFittingData;
				searchNode = chosenBlock = searchNode->right;
			}
			else
			{
				searchNode = searchNode->right;
			}
	}

	return chosenBlock;
}

unsigned int RBTMemoryAllocator::dbgCheckFreeTreeSanity() const
{
	unsigned int e = 0, count = 0;

	static Function<void(FreeHeader*, const Function<void(FreeHeader*)>&)> traverseBlock = [&e](FreeHeader* block, const Function<void(FreeHeader*)>& func)
	{
		if (!block)
		{
			return;
		}

		if (block->left == block)
		{
			e = 1;
			throw 0;
		}
		if (block->right == block)
		{
			e = 2;
			throw 0;
		}
		traverseBlock(block->left, func);
		func(block);
		traverseBlock(block->right, func);
	};

	if (getRed(freeMemory))
	{
		return 7;
	}

	if (freeMemory && (freeMemory->parent != nullptr))
	{
		return 8;
	}

	try
	{
		traverseBlock(freeMemory, [this, &e, &count](FreeHeader* block)
		{
			if (block->left)
			{
				if (cleanAddress(block->left->parent) != block)
				{
					e = 3;
					throw 0;
				}
			}

			if (block->right)
			{
				if (cleanAddress(block->right->parent) != block)
				{
					e = 4;
					throw 0;
				}
			}

			if (checkRedness(block->parent))
			{
				if (getRed(block->left) || getRed(block->right))
				{
					e = 5;
					throw 0;
				}
			}

			if ((block->left && !(block->right)) || (block->right && !(block->left)) || (!(block->left) && !(block->right)))
			{
				if (count != 0)
				{
					if (dbgGetBlackHeight(block) != count)
					{
						std::cout << "block's height: " << dbgGetBlackHeight(block) << " count: " << count << std::endl;
						e = 6;
						throw 0;
					}
				}
				else
				{
					count = dbgGetBlackHeight(block);
				}
			}
		});
	}
	catch (...)
	{
	}

	return e;
}

unsigned int RBTMemoryAllocator::dbgGetBlackHeight(FreeHeader * const head) const
{
	FreeHeader* search = head;
	unsigned int counter = 1;

	while (search->parent)
	{
		if (!checkRedness(search->parent))
		{
			++counter;
		}
		search = (FreeHeader*)cleanAddress(search->parent);
	}

	return counter;
}

void RBTMemoryAllocator::dbgWriteAllBlocks() const
{
	ClaimedHeader* search = trueMemoryBegin;

	std::cout << "Blocks dump:" << std::endl;
	std::cout << "FreeHeader: " << calcSize(search) << std::endl;
	search = cleanAddress(search->next);

	for (; search != cleanAddress(endOfMemory); search = cleanAddress(search->next))
	{
		if (isFreeHeader(cleanAddress(search->prev)->next))
		{
			std::cout << "FreeHeader: ";
		}
		else
		{
			std::cout << "ClaimedHeader: ";
		}
		std::cout << calcSize(search) << std::endl;
	}

	std::cout << std::endl;
}

bool RBTMemoryAllocator::dbgCheckListIntegrity() const
{
	ClaimedHeader* search = cleanAddress(trueMemoryBegin);

	while (search->next != endOfMemory)
	{
		if (cleanAddress(cleanAddress(search->next)->prev) != search)
		{
			return false;
		}
		search = cleanAddress(search->next);
	}

	return true;
}

RBTMemoryAllocator::RBTMemoryAllocator(_In_ const SizeType memorySize)
	: RBTMemoryAllocator(operator new(memorySize + mostRestrictiveAlignment), memorySize + mostRestrictiveAlignment, true)
{
}

RBTMemoryAllocator::RBTMemoryAllocator(_In_ void* memoryToUse, _In_ const SizeType memorySize, _In_opt_ const bool isOwning)
	: memory(memoryToUse), endOfMemory((ClaimedHeader*)addPointers(memoryToUse, memorySize)), totalMemory(0), freeMemory(nullptr), usedMemory(0), allocations(0), isOwningMemory(isOwning), trueMemoryBegin(nullptr)
{
	SizeType tempSize = memorySize;
	void* temp = memoryToUse;
	if (std::align(mostRestrictiveAlignment, memorySize - mostRestrictiveAlignment, temp, tempSize))
	{
		freeMemory = new (temp) FreeHeader;
		freeMemory->left = nullptr;
		freeMemory->right = nullptr;
		freeMemory->parent = nullptr;
		freeMemory->next = endOfMemory;
		freeMemory->prev = nullptr;
		trueMemoryBegin = freeMemory;
		totalMemory = calcSize(freeMemory);
	}
	else
	{
		throw std::bad_alloc();
	}
}

RBTMemoryAllocator::~RBTMemoryAllocator()
{
	if (allocations > 0)
	{
		throw std::runtime_error("Memory leak detected.");
	}

	if (isOwningMemory)
	{
		operator delete(memory);
	}
}

void * RBTMemoryAllocator::allocate(_In_ const SizeType howMany, _In_opt_ const SizeType alignment)
{
	// Assumption: two free blocks cannot be next to each other.

#ifdef RBMEM_CHECKSANITY
	if (!dbgCheckListIntegrity())
	{
		throw 8;
	}
#endif

	FittingBlockData fittingBlockData;
	FreeHeader* fittingBlock = findFittingBlock((howMany >= sizeof(FreeHeader) - sizeof(ClaimedHeader) ? howMany : sizeof(FreeHeader) - sizeof(ClaimedHeader)), alignment, fittingBlockData);

	if (!fittingBlock)
	{
		return nullptr;
	}

	removeFromRBTree(fittingBlock);
#ifdef RBMEM_CHECKSANITY
	dbgCheckSanity();
#endif

	ClaimedHeader headInfo = *fittingBlock;
	ClaimedHeader* const claimedBlock = new (fittingBlockData.usableMemory) ClaimedHeader;

	// Check the adjustment and remaining memory size.
	const SizeType adjustment = fittingBlockData.adjustment, remainingMemory = fittingBlockData.remainingMemory;

	// First we take care of back memory.
	if (remainingMemory >= sizeof(FreeHeader))
	{
		// Create new free block.
		//FreeHeader* newBlock = new (subPointers(cleanAddress(headInfo.next), remainingMemory)) FreeHeader;
		FreeHeader* newBlock = new (addPointers(claimedBlock, sizeof(ClaimedHeader) + fittingBlockData.usedMemory)) FreeHeader;
		newBlock->next = headInfo.next;
		if (headInfo.next != endOfMemory)
		{
			cleanAddress(headInfo.next)->prev = setIsFreeHeader(newBlock, true);
		}
		newBlock->prev = setIsFreeHeader(claimedBlock, false);
		claimedBlock->next = setIsFreeHeader(newBlock, true);
		insertToRBTree(newBlock);
#ifdef RBMEM_CHECKSANITY
		dbgCheckSanity();
#endif
	}
	else
	{
		// Add the remaining memory to the claimed block.
		claimedBlock->next = headInfo.next;
		if (headInfo.next != endOfMemory)
		{
			cleanAddress(headInfo.next)->prev = setIsFreeHeader(claimedBlock, false);
		}
	}

	// Take care of forward memory.
	if (adjustment >= sizeof(FreeHeader))
	{
		// Create new free block.
		FreeHeader* newBlock = new (fittingBlock) FreeHeader;
		newBlock->next = setIsFreeHeader(claimedBlock, false);
		newBlock->prev = headInfo.prev;
		claimedBlock->prev = setIsFreeHeader(newBlock, true);
		if (headInfo.prev)
		{
			cleanAddress(headInfo.prev)->next = setIsFreeHeader(newBlock, true);
		}
		insertToRBTree(newBlock);
#ifdef RBMEM_CHECKSANITY
		dbgCheckSanity();
#endif
	}
	else
	{
		// Add the remaining memory to previous block.
		claimedBlock->prev = headInfo.prev;
		if (headInfo.prev)
		{
			cleanAddress(headInfo.prev)->next = setIsFreeHeader(claimedBlock, false);
		}
	}

	++allocations;
	usedMemory += calcSize(claimedBlock);

#ifdef RBMEM_CHECKSANITY
	if (!dbgCheckListIntegrity())
	{
		throw 8;
	}
#endif

	//dbgWriteAllBlocks();
	return addPointers(claimedBlock, sizeof(ClaimedHeader));
	}

void RBTMemoryAllocator::deallocate(_In_ void* ptr)
{
	if (!ptr)
	{
		return;
	}

#ifdef RBMEM_CHECKSANITY
	if (!dbgCheckListIntegrity())
	{
		throw 8;
	}
#endif

	ClaimedHeader* claimedBlock = (ClaimedHeader*)subPointers(ptr, sizeof(ClaimedHeader));
	ClaimedHeader headInfo = *claimedBlock;
	usedMemory -= calcSize(claimedBlock);

	FreeHeader* newBlock = (FreeHeader*)claimedBlock;

	// Merge adjacent blocks if possible and add the new block to the RB tree.
	// Prevoius block (the one on left).
	if (isFreeHeader(headInfo.prev)) // If it doesn't work then try adding "&& cleanAddress(headInfo.prev) != nullptr".
	{
		newBlock = (FreeHeader*)cleanAddress(headInfo.prev);
		removeFromRBTree(newBlock);
#ifdef RBMEM_CHECKSANITY
		dbgCheckSanity();
#endif
		newBlock->next = headInfo.next;
	}

	// Next block (the one on right).
	if (isFreeHeader(headInfo.next) && headInfo.next != endOfMemory)
	{
		FreeHeader* tempHead = (FreeHeader*)cleanAddress(headInfo.next);
		removeFromRBTree(tempHead);
#ifdef RBMEM_CHECKSANITY
		dbgCheckSanity();
#endif
		newBlock->next = cleanAddress(headInfo.next)->next;
	}

	if (newBlock->prev)
	{
		cleanAddress(newBlock->prev)->next = setIsFreeHeader(newBlock, true);
	}
	if (newBlock->next != endOfMemory)
	{
		cleanAddress(newBlock->next)->prev = setIsFreeHeader(newBlock, true);
	}
	// Merging complete.

	newBlock->left = newBlock->right = newBlock->parent = nullptr;
	insertToRBTree(newBlock);
#ifdef RBMEM_CHECKSANITY
	dbgCheckSanity();
#endif

#ifdef RBMEM_CHECKSANITY
	if (!dbgCheckListIntegrity())
	{
		throw 8;
	}
#endif

	--allocations;
}
