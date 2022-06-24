#include <cstring>
#include  <unistd.h>
#include <cmath>
#include <sys/mman.h>
#include <iostream>

//#define ASSERT_TRUE(expr)  		\
//do {			 	 \
//	if((expr) == false) {	  \
//		std::cout << "Assertion Failed in " << __FILE__ << ":" << __LINE__  << std:: endl;	\
//		return false;       \
//	}			     \
//} while(0)


typedef struct MetaData {
    long size = 0;
    long is_free = 0;
    MetaData *next_by_size = nullptr;
    MetaData *next_by_address = nullptr;
    MetaData *prev_by_size = nullptr;
    MetaData *prev_by_address = nullptr;
} MetaData;

class MemoryBlocks {
private:
    MetaData *head_by_size = nullptr;
    MetaData *head_by_address = nullptr;
public:
    long total_blocks_counter = 0;
    long allocated_bytes_counter = 0;
    long free_blocks_counter = 0;
    long free_bytes_counter = 0;

    MemoryBlocks() = default;
    void incrementTotalBlocks(){ total_blocks_counter++; }
    void decreaseTotalBlocks() { total_blocks_counter--; }
    void incrementFreeBlocks(){ free_blocks_counter++; }
    void decreaseFreeBlocks() { free_blocks_counter--; }
    void increaseAllocatedBytes(long increaseBy){
        allocated_bytes_counter += increaseBy;
    }
    void increaseFreeBytes(long increaseBy){ free_bytes_counter += increaseBy; }

    void *allocateMap(long size){
        MetaData *block = (MetaData *) mmap(nullptr, sizeof(MetaData) + size, PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(block == MAP_FAILED){
            return nullptr;
        }

        block->size = size;
        block->is_free = false;

        incrementTotalBlocks();
        increaseAllocatedBytes(size + sizeof(MetaData));
        return block;
    }

    void *allocate(long size) {
        if(size > 128*1024){
            return allocateMap(size);
        }

        MetaData *curr_block = head_by_size;

        while (curr_block != nullptr) {
            if (curr_block->is_free && curr_block->size >= size) {
                if(curr_block->size >= size + sizeof(MetaData) + 128){
                    split(curr_block, size);
                }
                curr_block->is_free = false;
                decreaseFreeBlocks();
                increaseFreeBytes(-curr_block->size);
                return curr_block;
            }
            // If curr_block is the last in the list, exit and use it to chain the new block
            if (curr_block->next_by_size == nullptr) break;

            curr_block = curr_block->next_by_size;
        }

        curr_block = head_by_address;
        while(curr_block != nullptr && curr_block->next_by_address != nullptr){
            curr_block = curr_block->next_by_address;
        }

        if(curr_block != nullptr && curr_block->is_free){
            long allocate_extension = size - curr_block->size;
            if(sbrk((intptr_t) allocate_extension) == (void *) -1){
                return nullptr;
            }

            decreaseFreeBlocks();
            increaseFreeBytes(-curr_block->size);
            removeFromSizeList(curr_block);
            curr_block->size += allocate_extension;
            insertBySize(curr_block);
            curr_block->is_free = false;

            increaseAllocatedBytes(allocate_extension);

            return curr_block;
        }
        void *program_break = sbrk((intptr_t) (size + sizeof(MetaData)));
        if (program_break == (void *) -1) {
            return nullptr;
        }

        auto *block = (MetaData *) program_break;
        block->next_by_address = nullptr;
        block->prev_by_address = nullptr;
        block->next_by_size = nullptr;
        block->prev_by_size = nullptr;
        block->is_free = false;
        block->size = size;
        insertBySize(block);
        insertByAddress(block);

        incrementTotalBlocks();
        increaseAllocatedBytes(size);

        return block;
    }

    void removeFromSizeList(MetaData *block){
        if(block == nullptr) return;
        if(block == head_by_size){
            head_by_size = block->next_by_size;
            if(head_by_size != nullptr){
                head_by_size->prev_by_address = nullptr;
            }
            block->next_by_size = nullptr;
        } else {
            MetaData *prev = block->prev_by_size;
            MetaData *next = block->next_by_size;
            if(prev != nullptr){
                prev->next_by_size = next;
            }
            if(next != nullptr){
                next->prev_by_size = prev;
            }
            block->next_by_size = nullptr;
            block->prev_by_size = nullptr;
        }
    }

    void removeFromAddressList(MetaData *block){
        if(block == nullptr) return;
        if(block == head_by_address){
            head_by_address = block->next_by_address;
            if(head_by_address != nullptr){
                head_by_address->prev_by_address = nullptr;
            }
            block->next_by_address = nullptr;
        } else {
            MetaData *prev = block->prev_by_address;
            MetaData *next = block->next_by_address;
            if(prev != nullptr){
                prev->next_by_address = next;
            }
            if(next != nullptr){
                next->prev_by_address = prev;
            }
            block->next_by_address = nullptr;
            block->prev_by_address = nullptr;
        }
    }

    void split(MetaData *block, long size){
        long old_size = block->size;

        MetaData *split_block = (MetaData *) ((char *)block + size + sizeof(MetaData));
        split_block->size = old_size - size - sizeof(MetaData);
        split_block->is_free = true;

        split_block->prev_by_size = nullptr;
        split_block->prev_by_address = block;
        split_block->next_by_size = nullptr;
        split_block->next_by_address = block->next_by_address;
        block->next_by_address = split_block;
        block->is_free = false;
        block->size = size;
        if(split_block->next_by_address != nullptr && split_block->next_by_address->is_free){
            split_block = (MetaData *) mergeBlocks(split_block, split_block->next_by_address);
        }

        removeFromSizeList(block);

        insertBySize(block);
        insertBySize(split_block);

        incrementTotalBlocks();
        incrementFreeBlocks();
        increaseFreeBytes(- (long) sizeof(MetaData));
        increaseAllocatedBytes(- (long) sizeof(MetaData));
    }

    void *mergeBlocks(MetaData *left_block, MetaData *right_block){
        decreaseTotalBlocks();
        decreaseFreeBlocks();
        increaseAllocatedBytes(sizeof(MetaData));
        left_block->size += right_block->size + sizeof(MetaData);
        left_block->next_by_address = right_block->next_by_address;
        right_block->prev_by_address = nullptr;
        removeFromSizeList(right_block);
        removeFromAddressList(right_block);
        return left_block;
    }

    void insertBySize(MetaData *block){
        if(head_by_size == nullptr){
            block->prev_by_size = nullptr;
            block->next_by_size = nullptr;
            head_by_size = block;
            return;
        }
        MetaData *curr_block = head_by_size;
        if(block->size < curr_block->size || (block->size == curr_block->size && block < curr_block)){
            block->next_by_size = curr_block;
            curr_block->prev_by_size = block;
            head_by_size = block;
            return;
        }

        curr_block = head_by_size;
        while(curr_block != nullptr){
            if(block->size < curr_block->size || (block->size == curr_block->size && block < curr_block)){
                curr_block->prev_by_size->next_by_size = block;
                block->prev_by_size = curr_block->prev_by_size;
                curr_block->prev_by_size = block;
                block->next_by_size = curr_block;
                return;
            }
            if(curr_block->next_by_size == nullptr) break;
            curr_block = curr_block->next_by_size;
        }

        block->prev_by_size = curr_block;
        block->next_by_size = nullptr;
        curr_block->next_by_size = block;
    }

    void insertByAddress(MetaData *block){
        if(head_by_address == nullptr){
            block->prev_by_size = nullptr;
            block->next_by_size = nullptr;
            head_by_address = block;
            return;
        }
        MetaData *curr_block = head_by_address;
        if(block < curr_block){
            block->next_by_address = curr_block;
            curr_block->prev_by_address = block;
            head_by_address = block;
            return;
        }

        curr_block = head_by_address;
        while(curr_block != nullptr){
            if(block < curr_block){
                curr_block->prev_by_address->next_by_address = block;
                block->prev_by_address = curr_block->prev_by_address;
                curr_block->prev_by_address = block;
                block->next_by_address = curr_block;
                return;
            }
            if(curr_block->next_by_address == nullptr) break;
            curr_block = curr_block->next_by_address;
        }

        block->prev_by_address = curr_block;
        block->next_by_address = nullptr;
        curr_block->next_by_address = block;
    }
};

MemoryBlocks memory_blocks = MemoryBlocks();

long _num_free_blocks() {
    return memory_blocks.free_blocks_counter;
}

long _num_free_bytes() {
    return memory_blocks.free_bytes_counter;
}

long _num_allocated_blocks() {
    return memory_blocks.total_blocks_counter;
}

long _num_allocated_bytes() {
    return memory_blocks.allocated_bytes_counter;
}

long _num_meta_data_bytes() {
    return memory_blocks.total_blocks_counter * sizeof(MetaData);
}

long _size_meta_data() {
    return sizeof(MetaData);
}


void *smalloc(long size) {
    size = size % 8 != 0? size + (8 - size % 8) : size;

    if(size == 0 || size > (long) pow(10, 8)) return nullptr;

    void *program_break = memory_blocks.allocate(size);
    if(program_break == nullptr){
        return nullptr;
    }
    return (char *)program_break + sizeof(MetaData);
}

void *scalloc(long num, long size) {
    long actual_size = num*size;
    actual_size = actual_size % 8 != 0? actual_size + (8 - actual_size % 8) : actual_size;
    void *program_break = smalloc(actual_size);
    if(program_break == nullptr)
        return nullptr;

    memset(program_break, 0, actual_size);
    return program_break;
}

void sfree(void *p) {
    if(p != nullptr){
        MetaData *block = (MetaData* )((char *)p - sizeof(MetaData));
        if(block->is_free){
            return;
        }
        if(block->size >= 128*1024){
            memory_blocks.decreaseTotalBlocks();
            memory_blocks.increaseAllocatedBytes(-block->size);
            munmap(block, sizeof(MetaData) + block->size);
            return;
        }

        memory_blocks.incrementFreeBlocks();
        if(block->prev_by_address != nullptr && block->prev_by_address->is_free){
            memory_blocks.increaseFreeBytes(-block->prev_by_address->size);
            block = (MetaData *) memory_blocks.mergeBlocks(block->prev_by_address, block);
        }

        if(block->next_by_address != nullptr && block->next_by_address->is_free){
            memory_blocks.increaseFreeBytes(-block->next_by_address->size);
            block = (MetaData *) memory_blocks.mergeBlocks(block, block->next_by_address);
        }
        memory_blocks.increaseFreeBytes(block->size);
        block->is_free = true;

    }
}

void *srealloc(void *oldp, long size) {
    size += size % 8 != 0? size + (8 - size % 8) : size;
    if(size == 0 || size > (long) pow(10, 8)) return nullptr;
    if(oldp == nullptr) return smalloc(size);
    MetaData *old_block = (MetaData* )((char *)oldp - sizeof(MetaData));
    if(old_block->size >= size){
        return oldp;
    }

    MetaData *new_block;
    if(old_block->prev_by_address != nullptr && old_block->prev_by_address->is_free){
        new_block = (MetaData *) memory_blocks.mergeBlocks(old_block->prev_by_address, old_block);
        if(new_block->size > size + sizeof(MetaData) + 128){
            memory_blocks.split(new_block, size);
        }
        if(new_block->next_by_address == nullptr){
            if(new_block->size < size){
                long allocate_extension = size - new_block->size - sizeof(MetaData);
                if(sbrk((intptr_t) allocate_extension) == (void *) -1){
                    return nullptr;
                }
                new_block->size += allocate_extension;
            }
        }
        old_block = new_block;
        if(old_block->size >= size){
            memcpy(new_block, old_block, old_block->size);
            return new_block + sizeof(MetaData);
        }
    }

    if(old_block->next_by_address == nullptr){
        long allocate_extension = size - old_block->size - sizeof(MetaData);
        if(sbrk((intptr_t) allocate_extension) == (void *) -1){
            return nullptr;
        }
        old_block->size += allocate_extension;
        return oldp;
    }

    if(old_block->next_by_address != nullptr && old_block->next_by_address->is_free){
        new_block = (MetaData *) memory_blocks.mergeBlocks(old_block, old_block->next_by_address);
        if(new_block->size >  size + sizeof(MetaData) + 128){
            memory_blocks.split(new_block, size);
        }
        if(new_block->next_by_address == nullptr){
            if(new_block->size < size){
                long allocate_extension = size - new_block->size - sizeof(MetaData);
                if(sbrk((intptr_t) allocate_extension) == (void *) -1){
                    return nullptr;
                }
                new_block->size += allocate_extension;
            }
        }
        old_block = new_block;
        if(old_block->size >= size){
            memcpy(new_block, old_block, old_block->size);
            return new_block + sizeof(MetaData);
        }
    }

    void *program_break = smalloc(size);
    if(program_break == nullptr){
        //TODO: ???
    }
    memcpy(program_break, old_block, old_block->size);
    sfree(oldp);
    return program_break;
}
//
//bool isAlligned(void* addr) {
//    if(addr == nullptr) {
//        return false;
//    }
//    unsigned long num_addr = (unsigned long)(addr);
//    if(num_addr % 8 != 0) {
//        return false;
//    }
//    return true;
//}

//int main(int argc, char* argv[]) {
//    unsigned meta_size = _size_meta_data();
//    unsigned long base = (unsigned long)sbrk(0);
//    void* p1 = smalloc(131072);
//    ASSERT_TRUE(isAlligned(p1));
//    sfree(p1);
//    ASSERT_TRUE(_num_free_blocks() == 0);
//    ASSERT_TRUE(_num_free_bytes() == 0);
//    ASSERT_TRUE(_num_allocated_blocks() == 1);
//    ASSERT_TRUE(_num_allocated_bytes() == (131072 + meta_size));
//    unsigned long d1 = (unsigned long)sbrk(0);
//    ASSERT_TRUE((d1 - base) == 0);
//    return true;
//}
