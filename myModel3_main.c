//
//  myModel3_main.c
//  This model will simulate relations between two potentially unfriendly entities
//  A test/toy model for ROSS (Rensselaer Optimistic Simulation System
//  Created by Mark P. Blanco on 6/8/15.
//
//

#include "myModel3.h"
#include <stdio.h>
#include "ross.h"
#include <math.h>

#define RESOURCERATE 0.5
#define SIZE_SCALING 1.05
#define HEALTH_LIM 30
#define RESOURCE_LIM 10
#define DOWNSCALE_LIM 0
#define DEFAULT_SCALEUP 1
#define DEFAULT_EXPANSION 10
#define DEFAULT_DEMAND_AMT 7
#define NUMLPS 10

//Command Line Arguments
unsigned int setting_1 = 0;

// Initialize LPs (called by ROSS):
void init(state *s, tw_lp *lp){
    // Add content here.
    tw_event *current_event;
    tw_stime timestamp;
    message *new_message;
    // Initialize the state variables (maybe add random values for some of these later)
    s->health = 100;
    s->resources = 40;
    s->offense = 5;
    s->size = 30;
    s->at_war_with = -1;
    // Setup the first message (add/gather resources):
    // Send the message to be received immediately
    timestamp = 0;//tw_rand_exponential(lp->rng, 30);
    current_event = tw_event_new(lp->gid, timestamp, lp);
    new_message = tw_event_data(current_event);
    new_message->type = ADD_RESOURCES;
    new_message->sender = lp->gid;
    tw_event_send(current_event);
}

void event_handler(state *s, tw_bf *bf, message *input_msg, tw_lp *lp){
    int i;
    tw_lpid attack_gid;
    tw_event *current_event;
    tw_stime timestamp;
    message *new_message;
    // We know that events always happend exactly one timestep apart, so this isn't dangerous:
    timestamp = 1;

    switch (input_msg->type) {
        case DECLARE_WAR:
            if (s->at_war_with == -1){  // If we aren't at war yet
                war_field = 0;      // Set the bit field to record this
                // Update at_war_with status:
                s->at_war_with = input_msg->sender;
                if ( input_msg->damage < (s->health - HEALTH_LIM) && input_msg->demands < s->resources ){
                    fight_field = 1;
                    // Respond to the aggressor appropriately in a Fight message
                    // prepare a message to be received in the next tick
                    current_event = tw_event_new(input_msg->sender, timestamp, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = FIGHT;
                    new_message->demands = DEFAULT_DEMAND_AMT;
                    new_message->sender = lp->gid;
                    new_message->damage = s->offense;
                    tw_event_send(current_event);
                    // Send a scale_up message to self
                    current_event = tw_event_new(lp->gid, timestamp, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = SCALE_UP;
                    new_message->sender = lp->gid;
                    new_message->demands = DEFAULT_SCALEUP;
                    tw_event_send(current_event);
                } else {
                    fight_field = 0;
                    // Give the aggressor their demands in a make_peace message
                    if (s->resources >= input_msg->demands){
                        offer_field = 1;
                        s->resources-=input_msg->demands;
                        current_event = tw_event_new(input_msg->sender, timestamp, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = PROPOSE_PEACE;
                        new_message->sender = lp->gid;
                        new_message->offering = input_msg->demands;
                        tw_event_send(current_event);
                    } else {
                        offer_field = 0;
                        current_event = tw_event_new(input_msg->sender, timestamp, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = PROPOSE_PEACE;
                        new_message->sender = lp->gid;
                        new_message->offering = s->resources;
                        tw_event_send(current_event);
                        s->resources = 0;
                    }
                }
            } else {    // We are already at war with another entity, so use a make_peace as a way to let the second know we are already at war (simulation can't currently handle more than 1v1)
                war_field = 1;      // Although we set this field, nothing needs to be reversed
                current_event = tw_event_new(input_msg->sender, timestamp, lp);
                new_message = tw_event_data(current_event);
                new_message->type = PROPOSE_PEACE;
                new_message->sender = lp->gid;
                new_message->offering = 0;      // Since we are really just denying the declaration, don't give them anything.
                tw_event_send(current_event);
            }
            break;
            
        case PROPOSE_PEACE:
            if (input_msg->sender == s->at_war_with){
                // Note: we don't need to set any bitfield values since the else statement is irreversible (EXIT_FAILURE).
                // Accept the offering, reset at_war_with to -1, and respond appropriately
                s->at_war_with = -1;
                s->resources += input_msg->offering;
                current_event = tw_event_new(input_msg->sender, timestamp, lp);
                new_message = tw_event_data(current_event);
                new_message->type = ACCEPT_PEACE;
                new_message->sender = lp->gid;
                tw_event_send(current_event);
            } else {
                fprintf(stderr, "ERROR: Someone we weren't fighting with asked to make peace.\n");
                exit(EXIT_FAILURE);
            }
            break;
        case ACCEPT_PEACE:
            // No logic choices, thus no bit fields
            s->at_war_with = -1;    // Reset at_war_with to no-one (-1)
            break;
        case SCALE_UP:
            // No logic choices, thus no bit fields
            s->resources --;
            s->offense ++;
            break;
        case FIGHT:
            // Take damage from attack
            s->health -= input_msg->damage;
            // If health falls below zero, floor it for convenience in the simulation
            /*if (s->health < 0){
                s->health = 0;
            }*/
            // Now, based on current health and offensive capability compared to the enemy's, either surrender or fight back
            if ( (s->health-input_msg->damage) < HEALTH_LIM || s->offense <= input_msg->damage){
                fight_field = 0;
                current_event = tw_event_new(input_msg->sender, timestamp, lp);
                new_message = tw_event_data(current_event);
                new_message->type = PROPOSE_PEACE;
                new_message->sender = lp->gid;
                s->resources >= input_msg->demands
                new_message->offering = input_msg->demands;
                s->resources -= input_msg->demands;
                tw_event_send(current_event);
                // NOTE: the resources and health can both become negative. Should this happen, logic mechanisms will cause the entity to focus on rebuilding when next possible.
            } else {
                // Fight Back
                fight_field = 1;
                current_event = tw_event_new(input_msg->sender, timestamp, lp);
                new_message = tw_event_data(current_event);
                new_message->type = FIGHT;
                new_message->sender = lp->gid;
                new_message->damage = s->offense;
                new_message->demands = input_msg->demands;
                tw_event_send(current_event);
            }
            break;
        
        case REBUILD:
            // No logic choices, thus no bit fields
            // This is a message from past self.
            // Build up health at the cost of resources:
            s->resources --;
            s->health++;
            break;
        case SCALE_DOWN:
            // No logic choices, thus no bit fields
            // This is a message from past self
            s->offense--;
            s->resources++;
            break;
            
        case EXPAND:
            // This is a message from past self.
            s->size++;
            s->resources--;
            if (s->at_war_with == -1){
                war_field = 0;
                if (s->resources > RESOURCE_LIM && s->offense > DOWNSCALE_LIM || health < HEALTH_LIM){
                    attack_field = 0;
                    current_event = tw_event_new(lp->gid, timestamp, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = SCALE_DOWN;
                    new_message->sender = lp->gid;
                    tw_event_send(current_event);
                } else {
                    attack_field = 1;
                    // Randomly choose an lp to attack (who isn't us)
                    i = 0;
                    //attack_gid = (lp->gid + 1) % NUMLPS;
                    attack_gid = lp->gid;
                    while (attack_gid == lp->gid){
                        attack_gid = tw_rand_integer(lp->rng,0,NUMLPS-1);
                        i++;
                        if (i >= NUMLPS && attack_gid != lp->gid){
                            break;
                        }
                    }
                    s->at_war_with = attack_gid;
                    current_event = tw_event_new(s->at_war_with, timestamp, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = DECLARE_WAR;
                    new_message->sender = lp->gid;
                    new_message->demands = DEFAULT_DEMAND_AMT;
                    tw_event_send(current_event);
                }
            }
            break;
        case ADD_RESOURCES:
            s->resources += (int)(s->size*RESOURCERATE);
            // Note that casting to int will operate identically to math.floor for positive numbers
            // Add resources proportional to the size of the entity
            // This also ensures that the LP will keep simulating by always having an ADD_RESOURCES event to respond to
            current_event = tw_event_new(lp->gid, timestamp, lp);
            new_message = tw_event_data(current_event);
            new_message->type = ADD_RESOURCES;
            new_message->sender = lp->gid;
            tw_event_send(current_event);
            rebuild_field = 0;
            if (s->health < 100 && s->resources > RESOURCE_LIM){
                rebuild_field = 1;
                current_event = tw_event_new(lp->gid, timestamp, lp);
                new_message = tw_event_data(current_event);
                new_message->type = REBUILD;
                new_message->sender = lp->gid;
                tw_event_send(current_event);
            }
            expand_field = 0;
            else if (s->resources > RESOURCE_LIM) { //if we have the resources, send an expand message
                expand_field = 1;
                current_event = tw_event_new(lp->gid, timestamp, lp);
                new_message = tw_event_data(current_event);
                new_message->type = EXPAND;
                new_message->sender = lp->gid;
                tw_event_send(current_event);
            }
            break;
        default:
            fprintf(stderr, "ERROR: Unidentified forward message!\n");
            exit(EXIT_FAILURE);
            break;
    }
}

void event_handler_reverse(state *s, tw_bf *bf, message *input_msg, tw_lp *lp){
    // This is going to be a pain to write...probably
    
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
};

//add your command line opts
const tw_optdef model_opts[] = {
    TWOPT_GROUP("Versus Model"),
    TWOPT_STIME("remote", percent_remote, "desired remote event rate"),
    TWOPT_UINT("nlp", nlp_per_pe, "number of LPs per processor"),
    TWOPT_STIME("mean", mean, "exponential distribution mean for timestamps"),
    TWOPT_STIME("mult", mult, "multiplier for event memory allocation"),
    TWOPT_STIME("lookahead", lookahead, "lookahead for events"),
    TWOPT_UINT("start-events", g_phold_start_events, "number of initial messages per LP"),
    TWOPT_UINT("stagger", stagger, "Set to 1 to stagger event uniformly across 0 to end time."),
    TWOPT_UINT("memory", optimistic_memory, "additional memory buffers"),
    TWOPT_CHAR("run", run_id, "user supplied run name"),
    TWOPT_END()
};

#define myModel3_main main

// Main function
int myModel3_main(int argc, char *argv[]){
    int i;
    
    g_tw_memory_nqueues=1;
    nlp_per_pe = NUMLPS;
    lookahead=1.0;
    if( lookahead > 1.0 ) // Sanity check (from phold)
        tw_error(TW_LOC, "Lookahead > 1.0 .. needs to be less\n");
    
    tw_opt_add(model_opts);
    tw_init(&argc, &argv);
    
    //reset mean based on lookahead
    mean = mean - lookahead;
    
    g_tw_memory_nqueues = 16; // give at least 16 memory queue event
    
    offset_lpid = g_tw_mynode * nlp_per_pe;
    ttl_lps = tw_nnodes() * g_tw_npe * nlp_per_pe;
    g_tw_events_per_pe = (mult * nlp_per_pe * g_phold_start_events) +
				optimistic_memory;
    //g_tw_rng_default = TW_FALSE;
    
    g_tw_lookahead = lookahead;
    // Set up the LPs in ROSS:
    tw_define_lps(nlp_per_pe, sizeof(message), 0);
    for (i = 0; i < NUMLPS; i++){
        tw_lp_settype(i, &(model_lps[0]));
    }
    
    // The following block of output was taken from the phold model:
    if( g_tw_mynode == 0 )
    {
        printf("========================================\n");
        printf("VERSUS Model Configuration..............\n");
        printf("   Lookahead..............%lf\n", lookahead);
        printf("   Start-events...........%u\n", g_phold_start_events);
        printf("   stagger................%u\n", stagger);
        printf("   Mean...................%lf\n", mean);
        printf("   Mult...................%lf\n", mult);
        printf("   Memory.................%u\n", optimistic_memory);
        printf("   Remote.................%lf\n", percent_remote);
        printf("========================================\n\n");
    }
    
    
    tw_run();
    printf("IMPORTANT OUTPUT SHOULD GO HERE.");
    
    tw_end();
    
    return 0;
}

