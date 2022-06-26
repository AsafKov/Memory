#include <cstring>
#include  <unistd.h>
#include <cmath>
#include <sys/mman.h>
#include <iostream>

typedef struct MetaData {
    size_t size = 0;
    size_t is_free = 0;
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
    size_t total_blocks_counter = 0;
    size_t allocated_bytes_counter = 0;
    size_t free_blocks_counter = 0;
    size_t free_bytes_counter = 0;

    MemoryBlocks() = default;
    void incrementTotalBlocks(){ total_blocks_counter++; }
    void decreaseTotalBlocks() { total_blocks_counter--; }
    void incrementFreeBlocks(){ free_blocks_counter++; }
    void decreaseFreeBlocks() { free_blocks_counter--; }
    void increaseAllocatedBytes(size_t increaseBy){
        allocated_bytes_counter += increaseBy;
    }
    void decreaseAllocatedBytes(size_t decreaseBy){ allocated_bytes_counter -= decreaseBy;}
    void increaseFreeBytes(size_t increaseBy){
        free_bytes_counter += increaseBy;
    }
    void decreaseFreeBytes(size_t decreaseBy){
        free_bytes_counter -= decreaseBy;
    }

    void *allocateMap(size_t size){
        MetaData *block = (MetaData *) mmap(nullptr, sizeof(MetaData) + size, PROT_READ | PROT_WRITE,
                                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(block == MAP_FAILED){
            return nullptr;
        }

        block->size = size;
        block->is_free = false;

        incrementTotalBlocks();
        increaseAllocatedBytes(size);
        return block;
    }

    void *allocate(size_t size) {
        if(size >= 128*1024){
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
                decreaseFreeBytes(curr_block->size);
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
            size_t allocate_extension = size - curr_block->size;
            if(sbrk((intptr_t) allocate_extension) == (void *) -1){
                return nullptr;
            }

            decreaseFreeBlocks();
            decreaseFreeBytes(curr_block->size);
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

    void split(MetaData *block, size_t size){
        size_t old_size = block->size;

        MetaData *split_block = (MetaData *) ((char *)block + size + sizeof(MetaData));
        split_block->size = old_size - size - sizeof(MetaData);
        split_block->is_free = true;
        split_block->prev_by_size = nullptr;
        split_block->prev_by_address = block;
        split_block->next_by_size = nullptr;
        split_block->next_by_address = block->next_by_address;

        if(!block->is_free){
            increaseFreeBytes(old_size - size);
        }
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
        decreaseFreeBytes((size_t) sizeof(MetaData));
        decreaseAllocatedBytes((size_t) sizeof(MetaData));
    }

    void *mergeBlocks(MetaData *left_block, MetaData *right_block){
        MetaData *new_next = right_block->next_by_address;
        decreaseTotalBlocks();
        decreaseFreeBlocks();
        increaseAllocatedBytes(sizeof(MetaData));
        if(left_block->is_free){
            increaseFreeBytes(sizeof(MetaData));
        }
        left_block->size += right_block->size + sizeof(MetaData);
        left_block->next_by_address = new_next;
        if(new_next != nullptr){
            new_next->prev_by_address = left_block;
        }
        right_block->prev_by_address = nullptr;
        right_block->next_by_address = nullptr;
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

void split(MetaData *block, size_t size){
    if(block->size >  size + sizeof(MetaData) + 128){
        memory_blocks.split(block, size);
    }
}

size_t _num_free_blocks() {
    return memory_blocks.free_blocks_counter;
}

size_t _num_free_bytes() {
    return memory_blocks.free_bytes_counter;
}

size_t _num_allocated_blocks() {
    return memory_blocks.total_blocks_counter;
}

size_t _num_allocated_bytes() {
    return memory_blocks.allocated_bytes_counter;
}

size_t _num_meta_data_bytes() {
    return memory_blocks.total_blocks_counter * sizeof(MetaData);
}

size_t _size_meta_data() {
    return sizeof(MetaData);
}


void *smalloc(size_t size) {
    size = size % 8 != 0? size + (8 - size % 8) : size;

    if(size == 0 || size > (size_t) pow(10, 8)) return nullptr;

    void *program_break = memory_blocks.allocate(size);
    if(program_break == nullptr){
        return nullptr;
    }
    return (char *)program_break + sizeof(MetaData);
}

void *scalloc(size_t num, size_t size) {
    size_t actual_size = num*size;
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
            memory_blocks.decreaseAllocatedBytes(block->size);
            munmap(block, sizeof(MetaData) + block->size);
            return;
        }

        block->is_free = true;
        memory_blocks.increaseFreeBytes(block->size);
        memory_blocks.incrementFreeBlocks();
        if(block->prev_by_address != nullptr && block->prev_by_address->is_free){
            block = (MetaData *) memory_blocks.mergeBlocks(block->prev_by_address, block);
        }

        if(block->next_by_address != nullptr && block->next_by_address->is_free){
            block = (MetaData *) memory_blocks.mergeBlocks(block, block->next_by_address);
        }

    }
}

bool extendWilderness(MetaData *block, size_t extend_to){
    if(block->next_by_address == nullptr){
        if(block->size < extend_to){
            size_t allocate_extension = extend_to - block->size;
            if(sbrk((intptr_t) allocate_extension) == (void *) -1){
                return false;
            }
            memory_blocks.increaseAllocatedBytes(allocate_extension);
            block->size += allocate_extension;
        }
    }
    return true;
}

void *srealloc(void *oldp, size_t size) {
    size = size % 8 != 0? size + (8 - size % 8) : size;
    if(size == 0 || size > (size_t) pow(10, 8)) return nullptr;
    if(oldp == nullptr) return smalloc(size);
    MetaData *old_block = (MetaData* )((char *)oldp - sizeof(MetaData));
    if(old_block->size >= size){
        split(old_block, size);
        return oldp;
    }

    bool is_wilderness, next_is_wilderness, prev_free, prev_fits, next_free, next_fits, combined_fits;
    is_wilderness = old_block->next_by_address == nullptr;
    prev_free = old_block->prev_by_address != nullptr && old_block->prev_by_address->is_free;
    next_free = old_block->next_by_address != nullptr && old_block->next_by_address->is_free;
    next_is_wilderness = !is_wilderness && next_free && old_block->next_by_address->next_by_address == nullptr;
    prev_fits = prev_free && (is_wilderness || old_block->prev_by_address->size + old_block->size + sizeof(MetaData) >= size);
    next_fits = (next_free && old_block->next_by_address->size + old_block->size + sizeof(MetaData) >= size);
    combined_fits = prev_free && next_free && !prev_fits && !next_fits
            && old_block->prev_by_address->size + old_block->next_by_address->size + old_block->size + 2*sizeof(MetaData) >= size;

    MetaData *new_block;
    if(prev_fits || combined_fits || (next_is_wilderness && !next_fits && prev_free)){
        old_block->prev_by_address->is_free = false;
        memory_blocks.decreaseFreeBytes(old_block->prev_by_address->size);
        new_block = (MetaData *) memory_blocks.mergeBlocks(old_block->prev_by_address, old_block);
        new_block->is_free = false;
        split(new_block, size);
        if(!extendWilderness(new_block, size)){
            return nullptr; //sbrk failure
        }
        if(new_block->size >= size){
            memcpy(new_block, old_block, old_block->size);
            return (char *) new_block + sizeof(MetaData);
        }
        old_block = new_block;
    }


    if(!extendWilderness(old_block, size)){
        return nullptr; //sbrk failure
    } else {
        if(old_block->size == size){
            return  (char *) old_block + sizeof(MetaData);
        }
    }

    if(next_fits || combined_fits || next_is_wilderness){
        memory_blocks.decreaseFreeBytes(old_block->next_by_address->size);
        new_block = (MetaData *) memory_blocks.mergeBlocks(old_block, old_block->next_by_address);
        split(new_block, size);

        if(!extendWilderness(new_block, size)){
            return nullptr; //sbrk failure
        }
        if(new_block->size >= size){
            memcpy(new_block, old_block, old_block->size);
            return  (char *) new_block + sizeof(MetaData);
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