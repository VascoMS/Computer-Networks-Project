/******************************************************************************\
* Link state routing protocol.                                                 *
\******************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "routing-simulator.h"

typedef struct {
  cost_t link_cost[MAX_NODES];
  int version;
} link_state_t;

// Message format to send between nodes.
typedef struct {
  link_state_t ls[MAX_NODES];
} data_t;

// State format.
typedef struct {
  link_state_t ls[MAX_NODES];
} state_t;

void send_messages(){
  state_t *current_state = (state_t*) get_state();
  message_t message;
  message.data = (data_t*)malloc(sizeof(data_t));
  message.size = sizeof(data_t);
  memcpy(message.data, current_state->ls, sizeof(data_t));
  for(int n = get_first_node(); n <= get_last_node(); n = get_next_node(n)){
    if(get_link_cost(n) != COST_INFINITY && get_current_node() != n) 
      send_message(n, message);
  }
}


// Handler for the node to allocate and initialize its state.
void *init_state() {
  state_t *state = (state_t *)calloc(1, sizeof(state_t));
  for(int i = 0; i < MAX_NODES; i++){
    state->ls[i].version = 0;
    for(int j = 0; j < MAX_NODES; j++){
      if(i == j)
          state->ls[i].link_cost[j] = 0;
      else
        state->ls[i].link_cost[j] = COST_INFINITY;
    }
  }
  return state;
}



void djikstra(){
  state_t *current_state = (state_t *)get_state();
  cost_t distances[MAX_NODES];
  node_t hops[MAX_NODES];
  uint8_t visited[MAX_NODES] = {0};
  node_t current_n = get_current_node();
  memset(hops, -1, MAX_NODES*sizeof(node_t));
  memcpy(distances, current_state->ls[get_current_node()].link_cost, sizeof(cost_t)*MAX_NODES);
  visited[get_current_node()] = 1;

  for(node_t k = get_first_node(); k <= get_last_node(); k = get_next_node(k)){
    if(get_link_cost(k) < COST_INFINITY)
      hops[k] = k;
  }

  for(node_t n = get_first_node(); n <= get_last_node(); n = get_next_node(n)){
    cost_t min_cost = COST_INFINITY;
    for(node_t i = get_first_node(); i <= get_last_node(); i = get_next_node(i)){
      if(!visited[i] && distances[i] <= min_cost){
        current_n = i;
        min_cost = distances[i];
      }
    }
    visited[current_n] = 1;

    set_route(current_n, hops[current_n], distances[current_n]);

    for(int j = get_first_node(); j <= get_last_node(); j = get_next_node(j)){
      if(current_state->ls[current_n].link_cost[j] == COST_INFINITY) continue;

      if(!visited[j] && COST_ADD(current_state->ls[current_n].link_cost[j], distances[current_n]) < distances[j]){
        distances[j] = COST_ADD(current_state->ls[current_n].link_cost[j], distances[current_n]);
        hops[j] = hops[current_n];
      }
    }
  }
}

// Notify a node that a neighboring link has changed cost.
void notify_link_change(node_t neighbor, cost_t new_cost) {
  state_t *current_state = (state_t*) get_state();
  current_state->ls[get_current_node()].link_cost[neighbor] = new_cost;
  current_state->ls[get_current_node()].version++;
  djikstra();
  send_messages();
}

// Receive a message sent by a neighboring node.
void notify_receive_message(node_t sender, message_t message) {
  state_t *current_state = (state_t*) get_state();
  data_t *sender_state = (data_t*)message.data;
  int changed = 0;
  for(int i = get_first_node(); i <= get_last_node(); i = get_next_node(i)){
    if(sender_state->ls[i].version > current_state->ls[i].version){
      current_state->ls[i] = sender_state->ls[i];
      current_state->ls[i].version = sender_state->ls[i].version;
      changed = 1;
      djikstra();
    }
  }
  if(changed) send_messages();
}
