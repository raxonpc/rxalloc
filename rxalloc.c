#include <unistd.h>
#include <pthread.h>

typedef char ALIGN[16];

union header
{
    struct
    {
        union header *next;
        size_t size;
        unsigned short is_free;
    } s;
    ALIGN stub;
};
typedef union header header_t;

header_t *head, *tail;

pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;

header_t *get_free_block(size_t size)
{
    header_t *curr = head;
    while(curr)
    {
        if(curr->s.is_free && curr->s.size >= size)
            return curr;
        curr = curr->s.next;
    }
    return NULL;
}

void *malloc(size_t size)
{
    if(!size) return NULL;
    pthread_mutex_lock(&global_malloc_lock);

    header_t *header = get_free_block(size);
    if(header)
    {
        header->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void*)(header + 1);
    }

    size_t total_size = sizeof(header_t) + size;
    void* block = sbrk(total_size);
    if(block == (void*)-1)
    {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }
    
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;
    if(!head)
        head = header;
    if(tail)
        tail->s.next = header;
    tail = header;

    pthread_mutex_unlock(&global_malloc_lock);
    write(1, "malloc\n", 7);
    return (void*)(header + 1);
}

void free(void *block)
{
    if(!block) return;

    pthread_mutex_lock(&global_malloc_lock);
    header_t *header = (header_t*)block - 1;

    header_t *program_break = sbrk(0);
    if((char*)block + header->s.size == program_break)
    {
        if(head == tail)
        {
            head = tail = NULL;
        }
        else
        {
            header_t *tmp = head;
            while(tmp)
            {
                if(tmp->s.next == tail)
                {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
            }
        }
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

