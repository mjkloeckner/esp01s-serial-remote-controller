#include "queue.h"

void queue_init(queue_t *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    for (uint8_t i = 0; i < QUEUE_LEN; ++i)
    {
        queue->data[i] = 0;
    }
}

void queue_enqueue(queue_t *queue, uint8_t value)
{
    queue->data[queue->head] = value;
    queue->count += ((queue->count == QUEUE_LEN) ? 0 : 1);
    queue->head += ((queue->count == QUEUE_LEN) ? 0 : 1);
}

uint8_t queue_dequeue(queue_t *queue)
{
    if (queue_is_empty(queue))
    {
        return 0;
    }

    queue->count--;
    return queue->data[queue->tail++];
}

uint8_t queue_peek(queue_t *queue)
{
    return queue->data[queue->tail];
}

uint16_t queue_count(queue_t *queue)
{
    return queue->count;
}

uint8_t queue_is_empty(queue_t *queue)
{
    return (queue->count == 0);
}
