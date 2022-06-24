#ifndef MALLOC_MALLOC_2_H
#define MALLOC_MALLOC_2_H
#include  <unistd.h>
#include <cmath>

typedef struct MetaData{
    size_t size;
    bool is_free;
    MetaData *next_by_size;
    MetaData *prev_by_size;
} MetaData;

class MemoryBlocks{
private:
    MetaData *head_by_size = nullptr;
public:
    MemoryBlocks() = default;
    void* allocate(size_t size){
        MetaData *curr_block = head_by_size;
        while(curr_block != nullptr){
            if(curr_block->is_free && size <= curr_block->size){
                curr_block->is_free = false;
                return curr_block;
            }
            // If curr_block is the last in the list, exit and use it to chain the new block
            if(curr_block->next_by_size == nullptr) break;

            curr_block = curr_block->next_by_size;
        }
        void *program_break = sbrk((intptr_t) (size + sizeof(MetaData)));
        if(program_break == (void *) -1){
            return nullptr;
        }

        auto *block = (MetaData *) program_break;
        block->is_free = false;
        block->size = size;
        block->next_by_size = nullptr;
        block->prev_by_size = curr_block;

        return block;
    }
    size_t numOfFreeBlocks(){
        if(head_by_size == nullptr) return 0;
        size_t count = 0;
        MetaData *temp = head_by_size;
        while(temp != nullptr){
            count += temp->is_free;
            temp = temp->next_by_size;
        }

        return count;
    }
    size_t numOfFreeBytes(){
        if(head_by_size == nullptr) return 0;
        size_t count = 0;
        MetaData *temp = head_by_size;
        while(temp != nullptr){
            if(temp->is_free){
                count += temp->size;
            }
            temp = temp->next_by_size;
        }

        return count;
    }
    size_t numOfAllocatedBlocks(){
        if(head_by_size == nullptr) return 0;
        size_t count = 0;
        MetaData *temp = head_by_size;
        while(temp != nullptr){
            count += !temp->is_free;
            temp = temp->next_by_size;
        }

        return count;
    }
    size_t numOfAllocatedBytes(){
        if(head_by_size == nullptr) return 0;
        size_t count = 0;
        MetaData *temp = head_by_size;
        while(temp != nullptr){
            if(!temp->is_free){
                count += temp->size;
            }
            temp = temp->next_by_size;
        }

        return count;
    }
    size_t numOfMetaDataBytes(){
        if(head_by_size == nullptr) return 0;
        size_t count = 0;
        MetaData *temp = head_by_size;
        while(temp != nullptr){
            count += sizeof(MetaData);
            temp = temp->next_by_size;
        }

        return count;
    }
};

class malloc_2 {
    size_t _num_free_blocks();
    size_t _num_free_bytes();
    size_t _num_allocated_blocks();
    size_t _num_allocated_bytes();
    size_t _num_meta_data_bytes();
    size_t _size_meta_data();

public:
    void* smalloc(size_t size);
    void* scalloc(size_t num, size_t size);
    void sfree(void *p);
    void* srealloc(void* oldp, size_t size);
};




#endif //MALLOC_MALLOC_2_H
