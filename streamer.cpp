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

/* Writes data into the ring buffer */
void stream_write_ring_buffer(void * webm_cluster_table, void * data, long len) {
#if 0
    table_reference_information_t * tri = (table_reference_information_t*)webm_cluster_table;

    pthread_mutex_lock(&(tri->ring_read_mutex));
    long space_used = tri->ring_buffer_write_cursor-tri->ring_buffer_read_cursor;
    pthread_mutex_unlock(&(tri->ring_read_mutex));

    /* in case the cursor has wrapped, spaced used will be negative, so flip it */
    if (space_used <= -1) {
      space_used *= -1;
    }

    long space_free = tri->ring_buffer_len - space_used;

    /* confirm that we have enough room in the buffer */
    if (len > space_free) {
      printf("\n\nRING BUFFER: ran out of room!: %ld\n", space_free);
      exit (-4);
    }

    /* determine if we'll wrap around */
    pthread_mutex_lock(&(tri->ring_write_mutex));

    if (tri->ring_buffer_write_cursor + len > tri->ring_buffer_len) {
      /* Yup, split in two bits */
      long space_to_end_of_buffer = tri->ring_buffer_len - tri->ring_buffer_write_cursor;
      memcpy(tri->ring_buffer+tri->ring_buffer_write_cursor, data, space_to_end_of_buffer);
      tri->ring_buffer_write_cursor = 0;
      memcpy(tri->ring_buffer, data+space_to_end_of_buffer, len-space_to_end_of_buffer);
      tri->ring_buffer_write_cursor +=  len-space_to_end_of_buffer;
    } else {
      /* Straight copy */
      memcpy(tri->ring_buffer+tri->ring_buffer_write_cursor, data, len);
      tri->ring_buffer_write_cursor += len;
    }
    pthread_mutex_unlock(&(tri->ring_write_mutex));

    /* Notify the sender thread there's data in the ring */
    pthread_mutex_lock(&(tri->pending_send_mutex));
    tri->pending_data = 1;
    pthread_mutex_unlock(&(tri->pending_send_mutex));

    pthread_cond_signal(&(tri->pending_send_cond));
#endif
}

/* Retrieves up to the requested amount of data from the ring buffer */
long stream_read_ring_buffer(void * webm_cluster_table, void * data_out, long len_wanted) {
#if 0
  table_reference_information_t * tri = (table_reference_information_t*)webm_cluster_table;

  pthread_mutex_lock(&(tri->ring_read_mutex));

  /* Determine how much we can get on a read */
  long space_used = tri->ring_buffer_write_cursor-tri->ring_buffer_read_cursor;

  /* in case the cursor has wrapped, spaced used will be negative, so flip it */
  if (space_used <= -1) {
    space_used *= -1;
  }

  // If there's nothing in, nothing out :)
  if (space_used == 0) {
    pthread_mutex_unlock(&(tri->ring_read_mutex));
    return 0;
  }

  /* If we have less than what is wanted, that's what we return */
  if (space_used < len_wanted) {
    len_wanted = space_used;
  }

  /* Do we need to handle a wraparound ?*/
  if (tri->ring_buffer_read_cursor + len_wanted > tri->ring_buffer_len) {
    long space_to_end_of_buffer = tri->ring_buffer_len-tri->ring_buffer_read_cursor;
    memcpy(data_out, tri->ring_buffer+tri->ring_buffer_read_cursor, space_to_end_of_buffer);
    memcpy(data_out+space_to_end_of_buffer, tri->ring_buffer, len_wanted-space_to_end_of_buffer);
    tri->ring_buffer_read_cursor = len_wanted-space_to_end_of_buffer;
  } else {
    /* Straight memcpy */
    memcpy(data_out, tri->ring_buffer+tri->ring_buffer_read_cursor, len_wanted);
    tri->ring_buffer_read_cursor += len_wanted;
  }

  /* And done! */
 pthread_mutex_unlock(&(tri->ring_read_mutex));
 return len_wanted;
#endif
}

void* stream_send(void * webm_cluster_table) {
  table_reference_information_t * tri = (table_reference_information_t*)webm_cluster_table;

  return 0;
  UDTSOCKET client = UDT::socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(9000);
  inet_pton(AF_INET, "96.126.124.51", &serv_addr.sin_addr);

  memset(&(serv_addr.sin_zero), '\0', 8);

  // connect to the server, implict bind
  if (UDT::ERROR == UDT::connect(client, (sockaddr*)&serv_addr, sizeof(serv_addr))) {
	  std::cout << "connect:" << UDT::getlasterror().getErrorMessage() << std::endl;
	  exit(-2);
  }

  /* Connect to ingest */
#if 0
  int sock;
  struct sockaddr_in  dest;
  char post_message[1024];

  sock = socket(AF_INET, SOCK_STREAM, 0);
  memset(&dest, 0, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_addr.s_addr = inet_addr("96.126.124.51");
  dest.sin_port = htons(8082);

  int optval = 1;

  if((setsockopt(sock, SOL_SOCKET, TCP_NODELAY, &optval, sizeof(optval))) < 0) {
      perror("setsock failed");
  }

  if (connect (sock, (struct sockaddr*) &dest, sizeof(struct sockaddr))) {
    perror("Unable to connect to ingest!\n");
    exit(-2);
  }

  /* Send a POST message before sending data */
  strncat(post_message, "POST /in_dragon/1.webm HTTP/1.1\r\n", 1024);
  strncat(post_message, "User-Agent: vp8-live-streamer\r\n", 1024);
  strncat(post_message, "Connection: close\r\n", 1024);
  strncat(post_message, "Transfer-Encoding: chunked\r\n\r\n", 1024);

  int post_len = strnlen(post_message, 1024);
  int send_cursor = 0;
  while (send_cursor != post_len) {
    send_cursor = send(sock, post_message, post_len, 0);
    if (send_cursor < 0) {
      perror("send");
      exit (-1);
    }
  }
#endif

  for ( ; ; ) {
    /* Do the usual mutex dance for pending send data */
    pthread_mutex_lock(&(tri->pending_send_mutex));
    while (tri->pending_data == 0) {
      pthread_cond_wait(&(tri->pending_send_cond), &(tri->pending_send_mutex));
    }
    pthread_mutex_unlock(&(tri->pending_send_mutex));

    char buffer[10000];
    int stuff_to_send = stream_read_ring_buffer(webm_cluster_table, buffer, 10000);


    /* keep streaming until we run the buffer dry */
    while (stuff_to_send) {
      int send_cursor = 0;
      int post_len = 10000;
      int ss;

      while (send_cursor < stuff_to_send) {
    	  if (UDT::ERROR == (ss = UDT::send(client, buffer + send_cursor, stuff_to_send - send_cursor, 0)))
    	  {
    	    std::cout << "send:" << UDT::getlasterror().getErrorMessage() << std::endl;
    	    break;
    	  }

    	  send_cursor += ss;
      }
      stuff_to_send = stream_read_ring_buffer(webm_cluster_table, buffer, 10000);

      /*int return_code = send(sock, buffer, stuff_to_send, 0);
      if (return_code < 0) {
        perror("send errored out!");
        exit (-2);
      }
      //printf("\n\n%d\n\n", stuff_to_send);
      stuff_to_send = stream_read_ring_buffer(webm_cluster_table, buffer, 10000);*/
    }
  }
}

void* stream_prepare(void * webm_cluster_table) {
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

/*      header_prefix_len = snprintf((char*)transmission_buffer+buffer_cursor, 16, "%x", cursor->length);
      buffer_cursor += header_prefix_len;

      transmission_buffer[buffer_cursor] = 0x0d;
      buffer_cursor++;
      transmission_buffer[buffer_cursor] = 0x0a;
      buffer_cursor++;*/

      //memcpy(transmission_buffer, cursor->webm_cluster, cursor->length);
      //buffer_cursor += cursor->length;
      /*transmission_buffer[buffer_cursor] = 0x0d;
      buffer_cursor++;
      transmission_buffer[buffer_cursor] = 0x0a;
      buffer_cursor++;*/

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
      //free (transmission_buffer);
      if (cursor->webm_cluster) free(cursor->webm_cluster);
      cursor->webm_cluster = 0;
      cursor->length = 0;
      cursor->status = PAGE_FREE;
    }

    //stream_write_ring_buffer(webm_cluster_table, transmission_buffer, buffer_cursor);
    //send(sock, transmission_buffer, buffer_cursor, 0);

  }

  return NULL; /* NEVER_REACHED */
}
