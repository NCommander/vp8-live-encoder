/*
 * streamer.h
 *
 *  Created on: Oct 5, 2015
 *      Author: mcasadevall
 */

#ifndef STREAMER_H_
#define STREAMER_H_

#include <pthread.h>

typedef enum {
  PAGE_FREE,
  PAGE_VPX,
  PAGE_PENDING,
  PAGE_READY
} cluster_page_status_t;

#define SIZE_OF_ALLOCATION 16*1024*1024

#define NUM_OF_ALLOCATIONS 10000


typedef struct {
  long			id;
  int           status;
  int			length;
  unsigned char * webm_cluster;
} cluster_page_t;

typedef struct {
  int free_entries;
  int used_entries;
  int read_order[NUM_OF_ALLOCATIONS];
  int read_order_elements;
  int pending_work;
  int pending_data;
  void * ring_buffer;
  long ring_buffer_len;
  long ring_buffer_read_cursor;
  long ring_buffer_write_cursor;
  pthread_mutex_t ring_read_mutex;
  pthread_mutex_t ring_write_mutex;
  pthread_mutex_t webm_order_mutex;
  pthread_mutex_t pending_webm_mutex;
  pthread_mutex_t pending_send_mutex;
  pthread_cond_t  pending_webm_cond;
  pthread_cond_t  pending_send_cond;
  cluster_page_t  cluster_pages[NUM_OF_ALLOCATIONS];
} table_reference_information_t;


#define PAGE_SIZE (sizeof(cluster_page_t)*NUM_OF_ALLOCATIONS)

#ifdef __cplusplus
extern "C" {
#endif
void* streamer_init();
cluster_page_t * streamer_get_free_allocation(void * webm_cluster_table);
void stream_allocation_vpx_frame(void * webm_cluster_table, cluster_page_t *webm_cluster);
void stream_allocation_ready(void * webm_cluster_table, cluster_page_t *webm_cluster);
long stream_read_ring_buffer(void * webm_cluster_table, void * data_out, long len_wanted);

void* stream_send(void * webm_cluster_table);
void* stream_prepare(void * webm_cluster_table);

#ifdef __cplusplus
}
#endif
#endif /* STREAMER_H_ */
