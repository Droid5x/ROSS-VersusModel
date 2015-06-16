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
#include <stddef.h>

#define RESOURCERATE 0.16
#define SCALE_AMT 20
#define HEALTH_LIM 8
#define RESOURCE_LIM 0
#define DOWNSCALE_LIM 1
#define UPSCALE_LIM (100 - HEALTH_LIM)//2 maybe replace this with something to do with the new LP-specific health maximum...
#define DEFAULT_DEMAND_AMT 7
#define NUMLPS 5
#define DEBUG 1

final_stats * global_stats; // Used to properly print the stats for all the LPs

// Initialize LPs (called by ROSS):
void init(state *s, tw_lp *lp){
    tw_event *current_event;
    tw_stime timestamp;
    message *new_message;
    unsigned int value = rand();
    unsigned int times[] = {value, value, value, value};
    // Initialize the state variables randomly
    if (DEBUG) {
        s->health = 99;
        s->resources = 15;
        s->offense = 2;
        s->size = 5;
        s->at_war_with = -1;
        s->health_lim = 100;
    }
    else {
        rng_set_seed(lp->rng, times);
        s->health = tw_rand_integer(lp->rng, 15, 100);
        s->resources = tw_rand_integer(lp->rng, 20, 50);
        s->offense = tw_rand_integer(lp->rng, 1, 6);        // In order for the upper health limit to work
                                                            // we need to avoid divide by zero errors and generate numbers only from 1 to some upper bound.
        s->size = tw_rand_integer(lp->rng, 20, 36);
        s->at_war_with = -1;
        s->health_lim = (int)(s->size/s->offense);
    }
    s->times_defeated = 0;
    s->times_won = 0;
    s->wars_started = 0;
    // Setup the first message (add/gather resources):
    // Send the message to be received immediately in the next tick
    timestamp = 1;
    current_event = tw_event_new(lp->gid, tw_rand_integer(lp->rng, 1, 50), lp);
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
    timestamp = tw_rand_exponential(lp->rng,200);
    // Sanity Checks and Error Messages:
    if (s->resources < 0) fprintf(stderr, "ERROR: LP %llu has negative resources!\n", lp->gid);
    if (s->offense < 0) {
        fprintf(stderr, "ERROR: LP %llu has negative offense!\n", lp->gid);
        // Exit because this could have caused a divide by zero error.
        exit(EXIT_FAILURE);
    }
    // Onto Event Handling:
    switch (input_msg->type) {
        case DECLARE_WAR:               // We have just received a war declaration from someone.
            if (s->at_war_with == -1){  // We aren't at war yet with anyone else
                field0 = 0;             // Record this for the reverse handler
                fprintf(stderr, "NOTE: LP %llu just declared war on LP %llu.\n", input_msg->sender, lp->gid);
                s->at_war_with = input_msg->sender; // Update at_war_with new combatant
                if ( input_msg->damage < (s->health - HEALTH_LIM) ){    // Respond to the aggressor appropriately in a Fight message
                    field1 = 1;    // Record that we decided to fight
                    current_event = tw_event_new(input_msg->sender, timestamp + 1, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = FIGHT;
                    new_message->demands = DEFAULT_DEMAND_AMT;
                    new_message->sender = lp->gid;
                    new_message->damage = s->offense;
                    tw_event_send(current_event);
                    fprintf(stderr, "NOTE: LP %llu just retaliated war against LP %llu.\n", lp->gid, input_msg->sender);
                } else {
                    field1 = 0;
                    // Give the aggressor their demands in a make_peace message
                    if (s->resources >= input_msg->demands){
                        s->resources -= input_msg->demands;
                        current_event = tw_event_new(input_msg->sender,timestamp + 1, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = SURRENDER;
                        new_message->sender = lp->gid;
                        new_message->offering = input_msg->demands;
                        tw_event_send(current_event);
                    } else {
                        current_event = tw_event_new(input_msg->sender, timestamp + 1, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = SURRENDER;
                        new_message->sender = lp->gid;
                        new_message->offering = s->resources;
                        input_msg->demands = s->resources;      // Revise demanded resources for reverse handler
                        tw_event_send(current_event);
                        s->resources = 0;
                    }
                    s->times_defeated ++;
                    s->at_war_with = -1;    // Reset at_war_with to no-one (-1)
                }
            }
            else {    // We are already at war with another entity, so use a FORCE_PEACE as a way to let the second know we are already at war (simulation cannot handle more than 1v1)
                field0 = 1;      // Although we set this field, nothing needs to be reversed here
                current_event = tw_event_new(input_msg->sender, timestamp + 1, lp);
                new_message = tw_event_data(current_event);
                new_message->type = FORCE_PEACE;
                new_message->sender = lp->gid;
                new_message->offering = 0;      // Since we are really just denying the declaration, don't give anything.
                tw_event_send(current_event);
                fprintf(stderr, "WARNING: LP %llu declared war on LP %llu already occupied by LP %d\n", input_msg->sender, lp->gid, s->at_war_with);
            }
            break;
            
        case FIGHT:
            if (input_msg->sender == s->at_war_with && s->at_war_with != -1){   // Sanity check
                s->health -= input_msg->damage; // Take damage from attack
                // Now, based on current health and offensive capability compared to the enemy's, either surrender or fight back:
                if ( (s->health-input_msg->damage) < HEALTH_LIM ){
                    field1 = 0;
                    current_event = tw_event_new(input_msg->sender, timestamp + 1, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = SURRENDER;
                    new_message->sender = lp->gid;
                    // Pay as much of the demands as possible:
                    if (s->resources >= input_msg->demands){
                        new_message->offering = input_msg->demands;
                        s->resources -= input_msg->demands;
                    }
                    else {
                        input_msg->demands = s->resources;
                        new_message->offering = s->resources;
                        s->resources = 0;
                    }
                    tw_event_send(current_event);
                    s->at_war_with = -1;    // Reset at_war_with to no-one (-1)
                    s->times_defeated ++;
                }
                else {
                    // Fight Back
                    field1 = 1;
                    current_event = tw_event_new(input_msg->sender, timestamp + 1, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = FIGHT;
                    new_message->sender = lp->gid;
                    new_message->damage = s->offense;
                    new_message->demands = input_msg->demands;
                    tw_event_send(current_event);
                }
            }
            else {
                fprintf(stderr, "ERROR: LP %llu says someone we weren't fighting tried to fight without a declaration.\n",lp->gid);
                fprintf(stderr, "ERROR: \"enemy\" gid: %llu\n",input_msg->sender);
                fprintf(stderr, "ERROR: our at_war_with: %d \n", s->at_war_with);
                exit(EXIT_FAILURE);
            }
            break;
            
        case ADD_RESOURCES:
            // Note that casting to int will operate identically to math's floor function for positive numbers
            // Add resources proportional to the size of the entity
            input_msg->offering = (int)(s->size * tw_rand_unif(lp->rng) * RESOURCERATE);
            s->resources += input_msg->offering;
            // This also ensures that the LP will keep simulating by always having an ADD_RESOURCES event to respond to
            current_event = tw_event_new(lp->gid, timestamp + 1, lp);
            new_message = tw_event_data(current_event);
            new_message->type = ADD_RESOURCES;
            new_message->sender = lp->gid;
            tw_event_send(current_event);
            // Now we can do several of five things: scale up the arsenal, scale it down to recover resources,
            // rebuild (to regenerate health), declare war to get more resources if we aren't already
            // fighting, and expand our size/empire
            // All of these events will occur now, rather than being implemented as actual events to be
            // Received in the future
            field0 = 0;
            field1 = 0;
            field2 = 0;
            field3 = 0;
            field4 = 0;
            if (s->health < HEALTH_LIM) {   // Our health is low, so begin repairs
                field0 = 1;
                input_msg->damage = 0;
                while (s->health < s->health_lim && s->resources > RESOURCE_LIM) {
                    // TODO: maybe add a limit to how much one can rebuild in one 'turn'
                    input_msg->damage ++;
                    s->health ++;
                    s->resources --;
                }
            }
            if (s->at_war_with != -1) { // We are in a state of war, so arm up if we can
                field1 = 1;
                input_msg->demands = (int)(tw_rand_unif(lp->rng) * SCALE_AMT);
                s->offense += input_msg->demands;
                s->resources -= input_msg->demands;
            }
            else if ( (s->resources < RESOURCE_LIM || s->resources < (int)s->size/3) && s->offense > DOWNSCALE_LIM) {
                field1 = 1;
                field2 = 1;
                // If we don't have enough resources, we have two choices aside from waiting:
                // Scale down and recover resources, or declare war to take someone else's
                if (s->health < HEALTH_LIM) {   // Downscale
                    field3 = 1;
                    input_msg->demands = (int)(tw_rand_unif(lp->rng) * SCALE_AMT);
                    s->offense -= input_msg->demands;
                    s->resources += input_msg->demands;
                    if (s->offense < 0) {   // Safety check here
                        // (note that the math means we won't need to reverse the
                        // three lines below in the reverse handler)
                        s->resources += s->offense;
                        input_msg->demands += s->offense;
                        s->offense = 0;
                    }
                }
                else {  // Declare War on a random LP
                    // Randomly choose an lp to attack (who isn't us)
                    if (ttl_lps > 1) {  // Safety check
                        field4 = 1;
                        int attack_gid;
                        attack_gid = tw_rand_integer(lp->rng,0,ttl_lps-2);
                        if (attack_gid == lp->gid)
                            attack_gid++;
                        s->at_war_with = attack_gid;
                        // Generate and send the declaration
                        current_event = tw_event_new(attack_gid, timestamp + 1, lp);
                        new_message = tw_event_data(current_event);
                        new_message->type = DECLARE_WAR;
                        new_message->sender = lp->gid;
                        new_message->demands = DEFAULT_DEMAND_AMT;
                        new_message->damage = s->offense;
                        tw_event_send(current_event);
                        s->wars_started ++;
                        fprintf(stderr, "NOTE: LP %llu sent a war declaration on LP %d.\n", lp->gid, attack_gid);
                    }
                }
            }
            else if (s->resources > (int)s->size/3){   // We have nothing else to do, so expand the empire
                field2 = 1;
                    s->resources -= (int)s->size/3;
                    s->size ++;
                    s->health_lim ++;
            }
            break;
            
        case SURRENDER:
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
            
        case FORCE_PEACE:
            fprintf(stderr, "RESOLUTION: LP %llu backing off from attacking LP %llu. Before reset, at_war_with = %d.\n", lp->gid, input_msg->sender, s->at_war_with);
            s->at_war_with = -1;
            s->wars_started --;
            break;
            
        default:
            fprintf(stderr, "ERROR: Unidentified forward message!\n");
            exit(EXIT_FAILURE);
            break;
    }
}

void event_handler_reverse(state *s, tw_bf *bf, message *input_msg, tw_lp *lp){
    switch (input_msg->type) {
        case FORCE_PEACE:
            s->at_war_with = input_msg->sender;
            s->wars_started ++;
            break;
        case SURRENDER:
            s->times_won --;
            s->resources -= input_msg->offering;
            s->at_war_with = input_msg->sender;
            break;
        case ADD_RESOURCES:
            if (field1 && !field2) {
                s->offense -= input_msg->demands;
                s->resources += input_msg->demands;
                tw_rand_reverse_unif(lp->rng);
            }
            else if (field1 && field2) {
                if (field3) {
                    s->offense += input_msg->demands;
                    s->resources -= input_msg->demands;
                    tw_rand_reverse_unif(lp->rng);
                }
                else if (field4) {
                    s->at_war_with = -1;
                    s->wars_started --;
                    tw_rand_reverse_unif(lp->rng);
                }
            }
            else if (!field1 && field2) {
                s->health_lim --;
                s->size --;
                s->resources += (int)s->size/3;
            }
            if (field0) {
                s->health -= input_msg->damage;
                s->resources += input_msg->damage;
            }
            s->resources -= input_msg->offering;
            tw_rand_reverse_unif(lp->rng);
            break;
        case FIGHT:
            if (!field1) {
                s->resources += input_msg->demands;
                s->at_war_with = input_msg->sender;
                s->times_defeated --;
            }
            s->health += input_msg->damage;
            break;
        case DECLARE_WAR:
            if (!field0) {
                if (!field1) {
                    s->resources += input_msg->demands;
                    s->times_defeated --;
                }
                s->at_war_with = -1;
            }
            break;
        default:
            break;
    }
    tw_rand_reverse_unif(lp->rng);
    
}

// Place all of the LP stats in this rank's data struct array
void model_final_stats(state *s, tw_lp *lp){
    int index = (int)lp->gid % g_tw_nlp;
    global_stats[index].health = s->health;
    global_stats[index].health_lim = s->health_lim;
    global_stats[index].resources = s->resources;
    global_stats[index].offense = s->offense;
    global_stats[index].size = s->size;
    global_stats[index].at_war_with = s->at_war_with;
    global_stats[index].times_won = s->times_won;
    global_stats[index].times_defeated = s->times_defeated;
    global_stats[index].wars_started = s->wars_started;
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
    srand(time(NULL));
    int size, i, j;
    final_stats* buffer;
    
    g_tw_memory_nqueues=1;
    nlp_per_pe = NUMLPS;
    lookahead=1.0;
    if( lookahead > 1.0 ) // Sanity check (from phold)
        tw_error(TW_LOC, "Lookahead > 1.0 .. needs to be less\n");
    
    //tw_opt_add(model_opts);
    tw_init(&argc, &argv);
    
    //reset mean based on lookahead
    mean = mean - lookahead;
    
    g_tw_memory_nqueues = 16; // give at least 16 memory queue event
    
    offset_lpid = g_tw_mynode * nlp_per_pe;
    ttl_lps = tw_nnodes() * g_tw_npe * nlp_per_pe;
    g_tw_events_per_pe = (mult * nlp_per_pe * g_phold_start_events) + optimistic_memory;
    
    if (g_tw_mynode == 0) {
        buffer = malloc(sizeof(final_stats) * nlp_per_pe);
        // Define the actual size of the global stats array
        global_stats = malloc(sizeof(final_stats) * ttl_lps);
    }
    else {
        global_stats = malloc(sizeof(final_stats) * nlp_per_pe);
    }
    // Done with prep for final stats array
    
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
    
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    // Make the final_stats struct into an equivalent MPI version:
    const int num_items = 9;      // There are 9 items in the final_stats struct (all ints)
    int blocklengths[9] = {1,1,1,1,1,1,1,1,1};
    MPI_Datatype types[9] = {
        MPI_INT, MPI_INT,
        MPI_INT, MPI_INT,
        MPI_INT, MPI_INT,
        MPI_INT, MPI_INT,
        MPI_INT
    };
    MPI_Datatype mpi_final_stats;
    MPI_Aint offsets[9] = {
        offsetof(final_stats, health),
        offsetof(final_stats, health_lim),
        offsetof(final_stats, resources),
        offsetof(final_stats, offense),
        offsetof(final_stats, size),
        offsetof(final_stats, at_war_with),
        offsetof(final_stats, times_won),
        offsetof(final_stats, times_defeated),
        offsetof(final_stats, wars_started)
    };
    MPI_Type_create_struct(num_items, blocklengths, offsets, types, &mpi_final_stats);
    MPI_Type_commit(&mpi_final_stats);
    // Done creating mpi type

    tw_run();
    
    if (g_tw_mynode != 0){
        // Use MPI_Send to relay the final stats to the 0 process:
        MPI_Send(global_stats, nlp_per_pe, mpi_final_stats, 0, g_tw_mynode, MPI_COMM_WORLD);
    }
    else {
        // Receive and store the data:
        for (i = 1; i < size; i++) {
            MPI_Recv(buffer, nlp_per_pe, mpi_final_stats, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (j = 0; j < nlp_per_pe; j++) {
                global_stats[j + g_tw_nlp * i ] = buffer[j];
            }
        }
        for (i = 0; i < ttl_lps; i++) {
            if (DEBUG) {
                // Print all the LP's final state values here. (this is called for each LP)
                printf("\n\n====================================\n");
                printf("LP %d stats:\n", i);
                printf("Health:\t\t%d/%u (SHOULD BE 99/100)\n", global_stats[i].health, global_stats[i].health_lim);
                printf("Resources:\t%d (SHOULD BE 15)\n",global_stats[i].resources);
                printf("Offense:\t%d (SHOULD BE 2)\n", global_stats[i].offense);
                printf("Size:\t\t%d (SHOULD BE 5)\n", global_stats[i].size);
                if (global_stats[i].at_war_with > -1)
                    printf("At war with LP %d.\n", global_stats[i].at_war_with);
                else
                    printf("Not at war with any LP.\n");
                printf("Wars won:\t%u\n", global_stats[i].times_won);
                printf("Wars lost:\t%u\n", global_stats[i].times_defeated);
                printf("Wars started:\t%u\n", global_stats[i].wars_started);
                printf("====================================\n\n");
            }
            else {
                // Print all the LP's final state values here. (this is called for each LP)
                printf("\n\n====================================\n");
                printf("LP %d stats:\n", i);
                printf("Health:\t\t%d/%u\n", global_stats[i].health, global_stats[i].health_lim);
                printf("Resources:\t%d\n",global_stats[i].resources);
                printf("Offense:\t%d\n", global_stats[i].offense);
                printf("Size:\t\t%d\n", global_stats[i].size);
                if (global_stats[i].at_war_with > -1)
                    printf("At war with LP %d.\n", global_stats[i].at_war_with);
                else
                    printf("Not at war with any LP.\n");
                printf("Wars won:\t%u\n", global_stats[i].times_won);
                printf("Wars lost:\t%u\n", global_stats[i].times_defeated);
                printf("Wars started:\t%u\n", global_stats[i].wars_started);
                printf("====================================\n\n");
            }
        }
    }
    MPI_Type_free(&mpi_final_stats);
    tw_end();
    
    return 0;
}

