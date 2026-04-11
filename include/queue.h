
#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define QUEUE_LEN 255

typedef struct {
    uint8_t head;
    uint8_t tail;
    uint16_t count;
    uint8_t data[QUEUE_LEN];
} queue_t;

void queue_init(queue_t *queue);
void queue_enqueue(queue_t *queue, uint8_t value);
uint8_t queue_dequeue(queue_t *queue);
uint8_t queue_peek(queue_t *queue);
uint8_t queue_is_empty(queue_t *queue);
uint16_t queue_count(queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif // QUEUE_H
