//
//  myModel3.c
//  
//  A test/toy model for ROSS (Rensselaer Optimistic Simulation System
//  Created by Mark P. Blanco on 6/8/15.
//
//

#include "myModel3.h"
#include <stdio.h>
#include "ross.h"


// Initialize LPs (called by ROSS):s
void init(state *s, tw_lp *lp){
    // Add content here.
}

void event_handler(state *s, tw_bf *bf, message *input_msg, tw_lp *lp){
    // Add forward event handling here
}

void event_handler_reverse(state *s, tw_bf *bf, message *input_msg, tw_lp *lp){
    
}

void model_final_stats(state *s, tw_lp *lp){
    // Add satistic-collecting and reporting code here.
}

// Return the PE or node id given a gid
tw_peid model_map(tw_lpid gid){
    return (tw_peid) gid / g_tw_nlp;
}

tw_lptype model_lps[] = {
    {
        (init_f) init,
        (pre_run_f) NULL,
        (event_f) event_handler,
        (revent_f) event_handler_reverse,
        (final_f) model_final_stats,
        (map_f) model_map,
        sizeof(state)
    },
    { 0 },
}

//add your command line opts
const tw_optdef model_opts[] = {
    TWOPT_GROUP("ROSS Model"),
    TWOPT_UINT("setting_1", setting_1, "first setting for this model"),
    TWOPT_END(),
};

// Main function
int model_main(int argc, char **argv){
    
}

