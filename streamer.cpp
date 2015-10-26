/*
 * streamer.c
 *
 *  Created on: Oct 5, 2015
 *      Author: mcasadevall
 */

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/tcp.h>
#include <udt/udt.h>
#include <iostream>
#include "streamer.h"


/* Setups up shared memory and allocation table */
void * streamer_init() {
  void * webm_clusters = mmap(NULL, sizeof(table_reference_information_t),
      PROT_READ|PROT_WRITE, MAP_ANON|MAP_SHARED,
      -1, 0);

  table_reference_information_t * tri = (table_reference_information_t*)webm_clusters;
  pthread_mutex_init(&(tri->ring_read_mutex), NULL);
  pthread_mutex_init(&(tri->ring_write_mutex), NULL);

  pthread_mutex_init(&(tri->page_allocation_mutex), NULL);
  pthread_mutex_init(&(tri->webm_order_mutex), NULL);
  pthread_mutex_init(&(tri->pending_webm_mutex), NULL);
  pthread_mutex_init(&(tri->pending_send_mutex), NULL);

  pthread_cond_init(&(tri->pending_webm_cond), NULL);
  pthread_cond_init(&(tri->pending_send_cond), NULL);

  // Initialize the ring buffer to a nice large starting size
  tri->ring_buffer = malloc(SIZE_OF_ALLOCATION);
  if (tri->ring_buffer < 0) {
    printf("\n\nFAILED TO ALLOCATE RING!\n");
    exit(-5);
  }

  tri->ring_buffer_len = SIZE_OF_ALLOCATION;
  tri->ring_buffer_read_cursor = 0;
  tri->ring_buffer_write_cursor = 0;
  tri->pending_work = 0;
  tri->pending_data = 0;

  for (int i = 0; i != NUM_OF_ALLOCATIONS; i++) {
    cluster_page_t * cursor = &(tri->cluster_pages[i]);
    cursor->id = i;
    cursor->status = PAGE_FREE;
  }

  return webm_clusters;
}

/* Walks the cluster table and returns a free page */
cluster_page_t * streamer_get_free_allocation(void * webm_cluster_table) {
  table_reference_information_t * tri = (table_reference_information_t*)webm_cluster_table;

    pthread_mutex_lock(&(tri->page_allocation_mutex));
    for (int i = 0; i != NUM_OF_ALLOCATIONS; i++) {
      cluster_page_t * cursor = &(tri->cluster_pages[i]);
      if (cursor->status == PAGE_FREE) {
        cursor->status = PAGE_PENDING;
        pthread_mutex_unlock(&(tri->page_allocation_mutex));
        return cursor;
      }
    }

    /* eek, no free pages, bail out! */
    fprintf(stderr, "Ran out of allocations!\n");
    exit (-1);
}

/* When a cluster is ready, it goes to ready status, and is added FIFO queue for elements */
void stream_allocation_ready(void * webm_cluster_table, cluster_page_t *webm_cluster) {
    table_reference_information_t * tri = (table_reference_information_t*)webm_cluster_table;

    pthread_mutex_lock(&(tri->webm_order_mutex));
    webm_cluster->status = PAGE_READY;

    /* Read order tells the streamer in the order packets need to go out in */
    tri->read_order[tri->read_order_elements] = webm_cluster->id;
    tri->read_order_elements++;
    tri->pending_work = 1;
    pthread_mutex_unlock(&(tri->webm_order_mutex));
    pthread_cond_signal(&(tri->pending_webm_cond));
}


void* stream_process_webm_events(void * webm_cluster_table) {
    table_reference_information_t * tri = (table_reference_information_t*)webm_cluster_table;

    for ( ; ; ) {
      pthread_mutex_lock(&(tri->pending_webm_mutex));
      while (tri->pending_work == 0) {
        pthread_cond_wait(&(tri->pending_webm_cond), &(tri->pending_webm_mutex));
      }

      tri->pending_work = 0;
      pthread_mutex_unlock(&(tri->pending_webm_mutex));
    }
}

void* stream_send(void * webm_cluster_table) {
  table_reference_information_t * tri = (table_reference_information_t*)webm_cluster_table;
  int work_items[NUM_OF_ALLOCATIONS];
  int work_items_elements;

  UDTSOCKET client = UDT::socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(tri->ingest_port);
  inet_pton(AF_INET, tri->ip_address, &serv_addr.sin_addr);

  memset(&(serv_addr.sin_zero), '\0', 8);

  // connect to the server, implict bind
  if (UDT::ERROR == UDT::connect(client, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
	  std::cout << "connect:" << UDT::getlasterror().getErrorMessage() << std::endl;
	  exit(-2);
  }

  // Send the stream id
  char stream_command[255] = "STREAM \0";
  strcat(stream_command, tri->stream_id);
  printf("%s\n", stream_command);
  UDT::send(client, stream_command, strlen(stream_command), 0);


  for ( ; ; ) {
    pthread_mutex_lock(&(tri->pending_webm_mutex));
    while (tri->pending_work == 0) {
      pthread_cond_wait(&(tri->pending_webm_cond), &(tri->pending_webm_mutex));
    }

    tri->pending_work = 0;
    pthread_mutex_unlock(&(tri->pending_webm_mutex));

    /* Get the list of items to process */
    pthread_mutex_lock(&(tri->webm_order_mutex));
    memcpy(work_items, tri->read_order, sizeof(int)*NUM_OF_ALLOCATIONS);
    work_items_elements = tri->read_order_elements;

    memset(tri->read_order, 0, sizeof(int)*NUM_OF_ALLOCATIONS);
    tri->read_order_elements = 0;
    pthread_mutex_unlock(&(tri->webm_order_mutex));

    //char * transmission_buffer = (char*)malloc(work_items_elements*SIZE_OF_ALLOCATION);
   // memset(transmission_buffer, 0xc, work_items_elements*SIZE_OF_ALLOCATION);
    int buffer_cursor = 0;

    /* Send things in order to the client */
    for (int i = 0; i < work_items_elements; i++) {
      int header_prefix_len = 0;
      int work_item = work_items[i];
      int ssize = 0;
      int size = 0;
      int ss;

      cluster_page_t * cursor = &(tri->cluster_pages[work_item]);
      if (cursor->status != PAGE_READY || cursor->webm_cluster == 0) {
        printf("%ld page got in queue without being ready\n", cursor->id);
        goto cleanup;
      }
      if (cursor->length <= 0) {
        // This shouldn't happen, but libwebm has some hiccups with the
        // non standard cluster size
        goto cleanup;
      }

      size = cursor->length;
      while (ssize < size)
      {
        if (UDT::ERROR == (ss = UDT::send(client, cursor->webm_cluster + ssize, size - ssize, 0)))
        {
          std::cout << "send:" << UDT::getlasterror().getErrorMessage() << std::endl;
          exit(-1);
        }

        ssize += ss;
      }


      /* Return the page to the queue */
cleanup:
      if (cursor->webm_cluster) free(cursor->webm_cluster);
      cursor->webm_cluster = 0;
      cursor->length = 0;
      cursor->status = PAGE_FREE;
    }

  }

  return NULL; /* NEVER_REACHED */
}
