/******************************************************************************\
* Distance vector routing protocol without reverse path poisoning.             *
\******************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "routing-simulator.h"

// Message format to send between nodes.
typedef struct {
  cost_t distance_vector[MAX_NODES];
} data_t;

// State format.
typedef struct {
  cost_t distance_vectors[MAX_NODES][MAX_NODES];
} state_t;

void send_messages(){
  state_t *current_state = (state_t*) get_state();
  message_t message;
  message.data = (data_t*)malloc(sizeof(data_t));
  message.size = sizeof(data_t);
  memcpy(message.data, current_state->distance_vectors[get_current_node()], sizeof(data_t));
  for(int n = get_first_node(); n <= get_last_node(); n = get_next_node(n)){
    if(get_link_cost(n) != COST_INFINITY && get_current_node() != n) 
      send_message(n, message);
  }
}

// Handler for the node to allocate and initialize its state.
void *init_state() {
  state_t *state = (state_t *)calloc(1, sizeof(state_t));
  for(int i = 0; i < MAX_NODES; i++){
    for(int j = 0; j < MAX_NODES; j++){
      if(i == j)
          state->distance_vectors[i][j] = 0;
      else
        state->distance_vectors[i][j] = COST_INFINITY;
    }
  }
  return state;
}

int bellman_ford_recalc(){
  int changed = 0;
  state_t *current_state = (state_t*) get_state();
  for(node_t i = get_first_node(); i <= get_last_node(); i = get_next_node(i)){
    if(i == get_current_node()) continue;

    cost_t best_cost = COST_INFINITY;
    node_t best_hop = COST_INFINITY;

    for(node_t j = get_first_node(); j <= get_last_node(); j = get_next_node(j)){
      if(j == get_current_node() || get_link_cost(j) == COST_INFINITY) continue;

      if(COST_ADD(current_state->distance_vectors[j][i], get_link_cost(j)) < best_cost){
        best_cost = COST_ADD(current_state->distance_vectors[j][i], get_link_cost(j));
        best_hop = j;
      }
    }
    if(!changed && best_cost != current_state->distance_vectors[get_current_node()][i]) changed = 1;
    current_state->distance_vectors[get_current_node()][i] = best_cost;
    set_route(i, best_hop, best_cost);
  }
  return changed;
}

// Notify a node that a neighboring link has changed cost.
void notify_link_change(node_t neighbor, cost_t new_cost) {
  if(bellman_ford_recalc())
    send_messages();
}

// Receive a message sent by a neighboring node.
void notify_receive_message(node_t sender, message_t message) {
  state_t *current_state = (state_t*) get_state();
  memcpy(current_state->distance_vectors[sender], message.data, sizeof(data_t));
  if(bellman_ford_recalc())
    send_messages();
}
