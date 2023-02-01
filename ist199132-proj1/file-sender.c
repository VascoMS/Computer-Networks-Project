#include "packet-format.h"
#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int port = atoi(argv[1]);
  int window_size = atoi(argv[2]);
  printf("[server]window size: %d\n", window_size);

  // Prepare server socket.
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(-1);
  }

  // Allow address reuse so we can rebind to the same port,
  // after restarting the server.
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt");
    exit(-1);
  }
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
    perror("setsockopt");
    exit(-1);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = htonl(INADDR_ANY),
      .sin_port = htons(port),
  };
  if (bind(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) {
    perror("bind");
    exit(-1);
  }
  fprintf(stderr, "Receiving on port: %d\n", port);


  ssize_t len;
  struct sockaddr_in src_addr;
  req_file_pkt_t req_file_pkt;

  len = recvfrom(sockfd, &req_file_pkt, sizeof(req_file_pkt), 0,
                 (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});
  if (len < MAX_PATH_SIZE) {
    req_file_pkt.file_path[len] = '\0';
  }
  printf("[server]Received request for file %s, size %ld.\n", req_file_pkt.file_path,
         len);

  FILE *file = fopen(req_file_pkt.file_path, "r");
  if (!file) {
    perror("fopen");
    exit(-1);
  }



  ack_pkt_t latest_ack = {
    .seq_num= 0,
    .selective_acks = 0
  };
  uint32_t window_base = 0;
  uint32_t seq_num = 0;
  uint32_t last_seq_num = -1;
  int to_send = window_size;
  int successive_timeouts = 0;
  data_pkt_t data_pkt;
  size_t data_len;

  do { // Generate segments from file, until the the end of the file.
    printf("[server]New Loop, to_send: %d \n", to_send);  
    if(to_send > 0){
      // Prepare data segment.
      printf("[server]Preparing segment: %d\n", seq_num);
      data_pkt.seq_num = htonl(seq_num++);

      // Load data from file.
      data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
      printf("[server]read %ld\n", data_len);
      if(data_len < MAX_CHUNK_SIZE){
        last_seq_num = seq_num-1;
        to_send = 1;
        printf("[server]last seq num = %d \n", last_seq_num);
      }
      
      // Send segment.
      ssize_t sent_len =
          sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                (struct sockaddr *)&src_addr, sizeof(src_addr));
      printf("[server]Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
            offsetof(data_pkt_t, data) + data_len);
      if (sent_len != offsetof(data_pkt_t, data) + data_len) {
        fprintf(stderr, "Truncated packet.\n");
        exit(-1);
      }
      to_send--;
    }
    

    else{
      ack_pkt_t ack_pkt;
      len = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt_t), 0,
                  (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});
      printf("[server]Len = %d\n", (int)len);

      if(len == -1){
        //retransmit
        printf("[server]Timed out\n");
        successive_timeouts++;
        if(successive_timeouts == 3){
          printf("[server]3 succesive timeouts\n");
          exit(EXIT_FAILURE);
        }
        uint32_t sel_acks = latest_ack.selective_acks;
        int current_bit = 0;
        FILE *file_retransmit = fopen(req_file_pkt.file_path, "r");
        if (!file_retransmit) {
          perror("fopen");
          exit(-1);
        }
        while(current_bit < window_size && (last_seq_num == -1 || window_base + current_bit <= last_seq_num)) {
          if(current_bit == 0 || !(sel_acks & 0x0001)) {
            // Seek position in file
            fseek(file_retransmit, (window_base + current_bit) * MAX_CHUNK_SIZE, SEEK_SET);
            // Load data from file.
            data_pkt.seq_num = htonl(window_base + current_bit);
            data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file_retransmit);
            printf("[server]read(retransmit) %ld seq_num:%d\n", data_len, window_base + current_bit);

            // Send segment.
            sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                (struct sockaddr *)&src_addr, sizeof(src_addr));
            }
          if(current_bit > 0)
            sel_acks = sel_acks >> 1;
          current_bit++;

        }
      }
      else{
        successive_timeouts = 0;
        latest_ack.seq_num = ntohl(ack_pkt.seq_num);
        latest_ack.selective_acks = ntohl(ack_pkt.selective_acks);
        printf("[server]latest ack: seq_num: %d sel_ack: %d\n", latest_ack.seq_num, latest_ack.selective_acks);
        if(last_seq_num == -1)
          to_send = latest_ack.seq_num - window_base;
        printf("[server]to send after ack: %d\n", to_send);
        window_base = latest_ack.seq_num;
      }

    }
  } while (!(feof(file) && data_len < sizeof(data_pkt.data)) || latest_ack.seq_num != last_seq_num + 1);

  printf("[server]Finished\n");
  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return 0;
}
