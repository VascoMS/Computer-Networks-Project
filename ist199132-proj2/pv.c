/******************************************************************************\
* Path vector routing protocol.                                                *
\******************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "routing-simulator.h"

typedef struct{
  node_t route[MAX_NODES];
  cost_t total_cost;
} path_t;

// Message format to send between nodes.
typedef struct {
  path_t paths[MAX_NODES];
} data_t;

// State format.
typedef struct {
  path_t paths[MAX_NODES][MAX_NODES];
} state_t;

void send_messages(){
  state_t *current_state = (state_t*) get_state();
  message_t message;
  message.data = (data_t*)malloc(sizeof(data_t));
  message.size = sizeof(data_t);
  memcpy(message.data, current_state->paths[get_current_node()], sizeof(data_t));
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
      if(i == j){
          state->paths[i][j].total_cost = 0;
          state->paths[i][j].route[i] = (node_t)i;
      }
      else
        state->paths[i][j].total_cost = COST_INFINITY;
    }
  }
  return state;
}

int check_for_loop(node_t route[MAX_NODES], node_t first_n){
  int count[MAX_NODES];
  node_t n = first_n;
  memset(count, 0, sizeof(int)*MAX_NODES);
  count[get_current_node()] = 1;
  while(n != route[n]){
    count[n]++;
    if(count[n] > 1){
      return 0;
    }
    n = route[n];
  }
  return 1;
}

int bellman_ford_recalc(){
  int changed = 0;
  state_t *current_state = (state_t*) get_state();
  for(node_t i = get_first_node(); i <= get_last_node(); i = get_next_node(i)){
    if(i == get_current_node()) continue;

    cost_t best_cost = COST_INFINITY;
    node_t best_route[MAX_NODES];

    for(node_t j = get_first_node(); j <= get_last_node(); j = get_next_node(j)){
      if(j == get_current_node() || get_link_cost(j) == COST_INFINITY) continue;
      if(COST_ADD(current_state->paths[j][i].total_cost, get_link_cost(j)) < best_cost && check_for_loop(current_state->paths[j][i].route, j)){
        best_cost = COST_ADD(current_state->paths[j][i].total_cost, get_link_cost(j));
        memcpy(best_route, current_state->paths[j][i].route, sizeof(current_state->paths[j][i].route));
        best_route[get_current_node()] = j;
      }
    }
    if(!changed && (memcmp(best_route, current_state->paths[get_current_node()][i].route, sizeof(node_t)*MAX_NODES) || 
      current_state->paths[get_current_node()][i].total_cost != best_cost))
      changed = 1;
    memcpy(current_state->paths[get_current_node()][i].route, best_route, sizeof(node_t)*MAX_NODES);
    current_state->paths[get_current_node()][i].total_cost = best_cost;
    set_route(i, best_route[get_current_node()], best_cost);
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
  memcpy(current_state->paths[sender], message.data, sizeof(data_t));
  if(bellman_ford_recalc())
    send_messages();
}
