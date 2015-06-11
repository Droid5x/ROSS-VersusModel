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
#include <stdlib.h>
#include <time.h>

#define RESOURCERATE 0.16
#define HEALTH_LIM 8
#define RESOURCE_LIM 0
#define DOWNSCALE_LIM 1
#define UPSCALE_LIM (100 - HEALTH_LIM)//2 maybe replace this with something to do with the new LP-specific health maximum...
#define DEFAULT_DEMAND_AMT 7
#define NUMLPS 20

// Initialize LPs (called by ROSS):
void init(state *s, tw_lp *lp){
    tw_event *current_event;
    tw_stime timestamp;
    message *new_message;
    unsigned int value = rand();
    unsigned int times[] = {value, value, value, value};
    // Initialize the state variables randomly
    rng_set_seed(lp->rng, times);
    s->health = tw_rand_integer(lp->rng, 15, 100);
    s->resources = tw_rand_integer(lp->rng, 20, 50);
    s->offense = tw_rand_integer(lp->rng, 1, 6);        // In order for the upper health limit to work, we need to avoid divide by zero errors.
    s->size = tw_rand_integer(lp->rng, 5, 16);
    s->at_war_with = -1;
    s->health_lim = (int)(s->size/s->offense);
    s->times_defeated = 0;
    s->times_won = 0;
    s->wars_started = 0;
    // Setup the first message (add/gather resources):
    // Send the message to be received immediately
    timestamp = 1;//tw_rand_exponential(lp->rng, 30);
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
    timestamp = tw_rand_integer(lp->rng,2,6);   // May want to change this...
    if (s->resources < 0) fprintf(stderr, "ERROR: LP %llu has negative resources!\n", lp->gid);
    if (s->offense < 0) {
        fprintf(stderr, "ERROR: LP %llu has negative offense!\n", lp->gid);
        // Exit because this could have caused a divide by zero error anyways.
        exit(EXIT_FAILURE);
    }
    switch (input_msg->type) {
        case DECLARE_WAR:   // We have just received a war declaration from someone.
            if (s->at_war_with == -1){  // We aren't at war yet with anyone else
                war_field = 0;          // Set the bit field to record this for the reverse handler
                s->at_war_with = input_msg->sender; // Update at_war_with new combatant
                if ( input_msg->damage < (s->health - HEALTH_LIM) ){    // Respond to the aggressor appropriately in a Fight message
                    fight_field = 1;    // Record that we decided to fight
                    current_event = tw_event_new(input_msg->sender, timestamp, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = FIGHT;
                    new_message->demands = DEFAULT_DEMAND_AMT;
                    new_message->sender = lp->gid;
                    new_message->damage = s->offense;
                    tw_event_send(current_event);
                    // Send a scale_up message to self
                    if (s->resources > RESOURCE_LIM && s->offense < UPSCALE_LIM){
                        current_event = tw_event_new(lp->gid, 2, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = SCALE_UP;
                        new_message->sender = lp->gid;
                        tw_event_send(current_event);
                    }
                    fprintf(stderr, "NOTE: LP %llu just retaliated war against LP %llu.\n", lp->gid, input_msg->sender);
                } else {
                    fight_field = 0;
                    // Give the aggressor their demands in a make_peace message
                    if (s->resources >= input_msg->demands){
                        s->resources -= input_msg->demands;
                        current_event = tw_event_new(input_msg->sender, 1, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = PROPOSE_PEACE;
                        new_message->sender = lp->gid;
                        new_message->offering = input_msg->demands;
                        tw_event_send(current_event);
                    } else {
                        current_event = tw_event_new(input_msg->sender, 1, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = PROPOSE_PEACE;
                        new_message->sender = lp->gid;
                        new_message->offering = s->resources;
                        input_msg->demands = s->resources;      // Revise demanded resources for reverse handler (Note, makes use of bitfield unecessary.)
                        tw_event_send(current_event);
                        s->resources = 0;
                    }
                    s->times_defeated ++;
                    s->at_war_with = -1;    // Reset at_war_with to no-one (-1)
                }
            }
            else {    // We are already at war with another entity, so use a FORCE_PEACE as a way to let the second know we are already at war (simulation can't currently handle more than 1v1)
                war_field = 1;      // Although we set this field, nothing needs to be reversed here
                current_event = tw_event_new(input_msg->sender, 1, lp);
                new_message = tw_event_data(current_event);
                new_message->type = FORCE_PEACE;
                new_message->sender = lp->gid;
                new_message->offering = 0;      // Since we are really just denying the declaration, don't give anything.
                tw_event_send(current_event);
                fprintf(stderr, "WARNING: LP %llu declared war on LP %llu already occupied by LP %d\n", input_msg->sender, lp->gid, s->at_war_with);
            }
            break;
        case FORCE_PEACE:
            fprintf(stderr, "RESOLUTION: LP %llu backing off from attacking LP %llu. Before reset, at_war_with = %d.\n", lp->gid, input_msg->sender, s->at_war_with);
            s->at_war_with = -1;
            break;
            
        case PROPOSE_PEACE:
            if (input_msg->sender == s->at_war_with && s->at_war_with != -1){
                // Note: we don't need to set any bitfield values since the else statement is irreversible (EXIT_FAILURE).
                // Accept the offering and reset at_war_with to -1
                fprintf(stderr, "NOTE: LP %d just made peace with LP %llu.\n", s->at_war_with, lp->gid);
                s->at_war_with = -1;
                s->resources += input_msg->offering;
                s->times_won ++;
            }
            else {
                fprintf(stderr, "ERROR: LP %llu says: Someone we weren't fighting with asked to make peace.\n", lp->gid);
                fprintf(stderr, "ERROR: \"enemy\" gid: %llu\n",input_msg->sender);
                fprintf(stderr, "ERROR: our at_war_with: %d \n", s->at_war_with);
                exit(EXIT_FAILURE);
            }
            break;
        case SCALE_UP:
            // No logic choices, thus no bit fields needed for reverse handler
            fight_field = 0;
            if (s->resources > 0) { // Just a safety check
                fight_field = 1;
                s->resources --;
                s->offense ++;
            }
            break;
        case FIGHT:
            if (input_msg->sender == s->at_war_with && s->at_war_with != -1){   // Sanity check
                s->health -= input_msg->damage; // Take damage from attack
                // Now, based on current health and offensive capability compared to the enemy's, either surrender or fight back:
                if ( (s->health-input_msg->damage) < HEALTH_LIM ){
                    fight_field = 0;
                    current_event = tw_event_new(input_msg->sender, 1, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = PROPOSE_PEACE;
                    new_message->sender = lp->gid;
                    if (s->resources >= input_msg->demands){
                        offer_field = 1;
                        new_message->offering = input_msg->demands;
                        s->resources -= input_msg->demands;
                    }
                    else {
                        offer_field = 0;
                        new_message->offering = s->resources;
                        s->resources = 0;
                    }
                    tw_event_send(current_event);
                    s->at_war_with = -1;    // Reset at_war_with to no-one (-1)
                    s->times_defeated ++;
                }
                else {
                    // Fight Back
                    fight_field = 1;
                    current_event = tw_event_new(input_msg->sender, 1, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = FIGHT;
                    new_message->sender = lp->gid;
                    new_message->damage = s->offense;
                    new_message->demands = input_msg->demands;
                    tw_event_send(current_event);
                    // Send a scale_up message to self
                    if (s->resources > RESOURCE_LIM &&  s->offense < UPSCALE_LIM){
                        current_event = tw_event_new(lp->gid, 2, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = SCALE_UP;
                        new_message->sender = lp->gid;
                        tw_event_send(current_event);
                    }
                }
            }
            else {
                fprintf(stderr, "ERROR: Someone we weren't fighting tried to fight without a declaration.\n");
                fprintf(stderr, "ERROR: \"enemy\" gid: %llu\n",input_msg->sender);
                fprintf(stderr, "ERROR: our at_war_with: %d \n", s->at_war_with);
                exit(EXIT_FAILURE);
            }
            break;
        
        case REBUILD:
            // This is a message from past self.
            // Build up health at the cost of resources:
            input_msg->demands = 0;     // A rather hacky way to store the change in resources and health from the while loop so that it can be reversed in the rev. event handler
            // The third condition is an LP-specific limit so that full health regeneration isn't possible in one event call.
            while (s->resources > 0 && s->health < s->health_lim ){//&& input_msg->demands <= s->offense*2){
                input_msg->demands++;
                s->resources --;
                s->health ++;
            }
            break;
        case SCALE_DOWN:
            // No logic choices, thus no bit fields
            // This is a message from past self
            if (s->offense > 1){
                s->offense--;
                s->resources++;
            }
            break;
            
        case EXPAND:
            if (s->resources >= (int)(s->size/2)){
                // This is a message from past self.
                // The bigger you already are, the harder it is to expand
                s->resources -= (int)(s->size/2);
                s->size++;
                input_msg->damage = s->health_lim;  // Save the old health limit for the reverse handler
                s->health_lim = (int)(s->size/(s->offense+10)); // We'll see if this causes any interesting behavior...
                if (s->at_war_with == -1){
                    war_field = 0;
                    if ( (s->health < HEALTH_LIM && s->offense > 0) ){
                        attack_field = 0;
                        current_event = tw_event_new(lp->gid, 1, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = SCALE_DOWN;
                        new_message->sender = lp->gid;
                        tw_event_send(current_event);
                    } else if (s->resources < RESOURCE_LIM || s->health == s->health_lim){
                        attack_field = 1;
                        i = 0;
                        // Randomly choose an lp to attack (who isn't us)
                        attack_gid = lp->gid;
                        attack_gid = tw_rand_integer(lp->rng,0,NUMLPS-2);
                        if (attack_gid == lp->gid)
                            attack_gid++;
                        s->at_war_with = attack_gid;
                        current_event = tw_event_new(attack_gid, timestamp, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = DECLARE_WAR;
                        new_message->sender = lp->gid;
                        new_message->demands = DEFAULT_DEMAND_AMT;
                        new_message->damage = s->offense;
                        tw_event_send(current_event);
                        fprintf(stderr, "NOTE: LP %llu just declared war on LP %llu.\n", lp->gid, attack_gid);
                        s->wars_started ++;
                    }
                }
            }
            break;
        case ADD_RESOURCES:
            s->resources += (int)(s->size*tw_rand_unif(lp->rng)*RESOURCERATE);
            // Note that casting to int will operate identically to math.floor for positive numbers
            // Add resources proportional to the size of the entity
            // This also ensures that the LP will keep simulating by always having an ADD_RESOURCES event to respond to
            current_event = tw_event_new(lp->gid, 3, lp);
            new_message = tw_event_data(current_event);
            new_message->type = ADD_RESOURCES;
            new_message->sender = lp->gid;
            tw_event_send(current_event);
            rebuild_field = 0;
            expand_field = 0;
            if (s->health < s->health_lim && s->resources > 0){
                rebuild_field = 1;
                current_event = tw_event_new(lp->gid, 1, lp);
                new_message = tw_event_data(current_event);
                new_message->type = REBUILD;
                new_message->sender = lp->gid;
                tw_event_send(current_event);
            }
            else if (s->resources > (int)s->size) { //if we have the resources, send an expand message
                expand_field = 1;
                current_event = tw_event_new(lp->gid, 1, lp);
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
    tw_rand_reverse_unif(lp->rng);
    switch (input_msg->type) {
        case DECLARE_WAR:
            if (!war_field){
                if (!fight_field)
                    s->resources+=input_msg->demands;
                else
                    s->times_defeated --;
            }
            break;
        case FORCE_PEACE:
            s->at_war_with = input_msg->sender;
        case PROPOSE_PEACE:
            s->at_war_with = input_msg->sender;
            s->resources -= input_msg->offering;
            s->times_won --;
            break;
        case SCALE_UP:
            if (fight_field) {
                s->resources ++;
                s->offense --;
            }
            break;
        case FIGHT:
            s->health += input_msg->damage;
            if (!fight_field){
                s->resources += input_msg->demands;
                s->times_defeated --;
            }
            break;
        case REBUILD:
            s->health -= input_msg->demands;
            s->resources += input_msg->demands;
            break;
        case SCALE_DOWN:
            s->resources --;
            s->offense ++;
            break;
        case EXPAND:
            s->size --;
            s->resources += (int)(s->size/2);
            s->health_lim = input_msg->damage;
            if (!war_field && attack_field) {
                s->at_war_with = -1;
                s->wars_started --;
                tw_rand_reverse_unif(lp->rng);
            }
            break;
        case ADD_RESOURCES:
            tw_rand_reverse_unif(lp->rng);
            s->resources -= (int)(s->size*tw_rand_unif(lp->rng)*RESOURCERATE);
            tw_rand_reverse_unif(lp->rng);
            break;
        default:
            break;
    }
    
}

void model_final_stats(state *s, tw_lp *lp){
    // Print all the LP's final state values here. (this is called for each LP)
    printf("\n\n====================================\n");
    printf("LP %llu stats:\n", lp->gid);
    printf("Health:\t\t%d/%u\n", s->health, s->health_lim);
    printf("Resources:\t%d\n",s->resources);
    printf("Offense:\t%d\n", s->offense);
    printf("Size:\t\t%d\n", s->size);
    if (s->at_war_with > -1)
        printf("At war with LP %d.\n", s->at_war_with);
    else
        printf("Not at war with any LP.\n");
    printf("Wars won:\t%u\n", s->times_won);
    printf("Wars lost:\t%u\n", s->times_defeated);
    printf("Wars started:\t%u\n", s->wars_started);
    printf("====================================\n\n");
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

// Command line opts:
// TODO: make these parameters actually useful and relevant to this model
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
    srand(time(NULL));
    
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
    
    tw_end();
    
    return 0;
}

