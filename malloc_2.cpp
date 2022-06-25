#include <cstring>
#include  <unistd.h>
#include <cmath>

typedef struct MetaData{
    size_t size = 0;
    bool is_free = false;
    MetaData *next_by_size = nullptr;
    MetaData *prev_by_size = nullptr;
} MetaData;

class MemoryBlocks{
private:
    MetaData *head_by_size = nullptr;
public:
    size_t total_blocks_counter = 0;
    size_t allocated_bytes_counter = 0;
    size_t free_blocks_counter = 0;
    size_t free_bytes_counter = 0;

    MemoryBlocks() = default;
    void* allocate(size_t size) {
        MetaData *curr_block = head_by_size;
        while (curr_block != nullptr) {
            if (curr_block->is_free && size <= curr_block->size) {
                free_bytes_counter -= curr_block->size;
                free_blocks_counter--;
                curr_block->is_free = false;
                return curr_block;
            }
            // If curr_block is the last in the list, exit and use it to chain the new block
            if (curr_block->next_by_size == nullptr) break;

            curr_block = curr_block->next_by_size;
        }
        void *program_break = sbrk((intptr_t) (size + sizeof(MetaData)));
        if (program_break == (void *) -1) {
            return nullptr;
        }
        total_blocks_counter++;
        allocated_bytes_counter += size;
        auto *block = (MetaData *) program_break;
        block->is_free = false;
        block->size = size;
        insertBySize(block);
        return block;
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
};

MemoryBlocks memory_blocks = MemoryBlocks();

void *smalloc(size_t size) {
    if(size == 0 || size > (long) pow(10, 8)) return nullptr;

    void *program_break = memory_blocks.allocate(size);
    if(program_break == nullptr){
        return nullptr;
    }
    return (char *)program_break + sizeof(MetaData);
}

void *scalloc(size_t num, size_t size) {
    void *program_break = smalloc(num * size);
    if(program_break == nullptr)
        return nullptr;

    memset(program_break, 0, num*size);
    return program_break;
}

void sfree(void *p) {
    if(p != nullptr){
        MetaData *block = (MetaData* )((char *)p - sizeof(MetaData));
        if(block->is_free) return;
        memory_blocks.free_blocks_counter++;
        memory_blocks.free_bytes_counter += block->size;
        block->is_free = true;
    }
}

void *srealloc(void *oldp, size_t size) {
    if(size == 0 || size > (long) pow(10, 8)) return nullptr;
    if(oldp == nullptr) return smalloc(size);

    MetaData *old_block = (MetaData* )((char *)oldp - sizeof(MetaData));
    if(old_block->size > size) return old_block;

    void *program_break = smalloc(size);
    memcpy(program_break, old_block, old_block->size);
    sfree(oldp);
    return program_break;
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
