#include "packet-format.h"
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int find_last_path_separator(char* path) {
  int last_found_pos = -1;
  int curr_pos = 0;
  

  while (*path != '\0') {
    if (*path == '/') {
      last_found_pos = curr_pos;
    }

    path += 1;
    curr_pos += 1;
  }

  return last_found_pos;
}


int main(int argc, char *argv[]) {
  char *file_path = argv[1];
  char *host = argv[2];
  int port = atoi(argv[3]);
  int window_size = atoi(argv[4]);

  int last_path_sep_index = find_last_path_separator(file_path);
  char *file_name = file_path;

  if (last_path_sep_index != -1 && last_path_sep_index < MAX_PATH_SIZE - 1) {
    file_name = file_path + last_path_sep_index + 1;
  }

  FILE *file = fopen(file_name, "w");
  if (!file) {
    perror("fopen");
    exit(-1);
  }

  // Prepare server host address.
  struct hostent *he;
  if (!(he = gethostbyname(host))) {
    perror("gethostbyname");
    exit(-1);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = *((struct in_addr *)he->h_addr),
  };

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(-1);
  }

  struct timeval tv;
  tv.tv_sec = 4;
  tv.tv_usec = 0;
  if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
    perror("setsockopt");
    exit(-1);
  }

  req_file_pkt_t req_file_pkt;
  size_t file_path_len = strlen(file_path);
  strncpy(req_file_pkt.file_path, file_path, file_path_len);

  ssize_t sent_len = sendto(sockfd, &req_file_pkt, file_path_len, 0,
                            (struct sockaddr *)&srv_addr, sizeof(srv_addr));
  
  

  if (sent_len != file_path_len) {
    fprintf(stderr, "Truncated packet.\n");
    exit(-1);
  }
  printf("[client]Sending request for file %s, size %ld.\n", file_path, file_path_len);

  uint32_t selective_ack = 0;
  uint32_t window_base = 0;
  ssize_t len;
  ack_pkt_t ack;
  do { // Iterate over segments, until last the segment is detected.
    // Receive segment.
    data_pkt_t data_pkt;
    struct sockaddr_in src_addr;

    printf("[client]Checking socket\n");
    len =
        recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                 (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});

    if(len == -1){
      remove(file_name);
      exit(EXIT_FAILURE);
    }


    printf("[client]Received segment %d, size %ld.\n", ntohl(data_pkt.seq_num), len);

    data_pkt.seq_num = ntohl(data_pkt.seq_num);

    if(window_base <= data_pkt.seq_num && data_pkt.seq_num < window_base + window_size){
      printf("[client]seeking in client\n");
      fseek(file, data_pkt.seq_num*MAX_CHUNK_SIZE, SEEK_SET);
      // Write data to file.
      fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);
      if(data_pkt.seq_num == window_base){
        int current_bit = 1;
        while(current_bit < window_size && (selective_ack & 0x0001)){
          current_bit++;
          selective_ack = selective_ack >> 1;
        }
        selective_ack = selective_ack >> 1;
        window_base += current_bit;
        printf("[client] new window base: %d\n", window_base);
      }
      else if(data_pkt.seq_num > window_base)
        selective_ack = selective_ack | (1 << (data_pkt.seq_num - window_base - 1));
    }

    ack.seq_num = htonl(window_base),
    ack.selective_acks = htonl(selective_ack),
    
    sendto(sockfd, &ack, sizeof(ack_pkt_t), 0,
               (struct sockaddr *)&srv_addr, sizeof(srv_addr));
    printf("[client]sent ack for- seq_num:%d sel: %d\n", window_base, selective_ack);
        
    
  } while (len == sizeof(data_pkt_t) || selective_ack != 0);

  do{
    data_pkt_t data_pkt;
    struct sockaddr_in src_addr;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
      perror("setsockopt");
      exit(-1);
    }
    len =
        recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                 (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});
    if(len != -1)
      sendto(sockfd, &ack, sizeof(ack_pkt_t), 0,
               (struct sockaddr *)&srv_addr, sizeof(srv_addr));


    }while(len != -1);

  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return 0;
}
