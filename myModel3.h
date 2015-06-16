//
//  myModel3.h - This model will be called VERSUS
//  This model will simulate relations between two potentially unfriendly entities
//  A test/toy model for ROSS (Rensselaer Optimistic Simulation System
//  Created by Mark P. Blanco on 6/8/15.
//
//

#ifndef ____myModel3__
#define ____myModel3__

#include <stdio.h>
#include "ross.h"
// defines to more easily handle the bit field locations:
#define field0 bf->c0
#define field1 bf->c1
#define field2 bf->c2
#define field3 bf->c3
#define field4 bf->c4
#define field5 bf->c5

// Define message types here
typedef enum {
    DECLARE_WAR,
    FORCE_PEACE,    // This event only exists because LPs can otherwise declare war
                    // on already occupied LPs
    SURRENDER,
    FIGHT,       //(Note that the defender always gets the first strike)
    ADD_RESOURCES
} message_type;

// Define message data here:
typedef struct {
    message_type type;
    tw_lpid sender;
    long int damage;     // The damage caused by the sender
    long int demands;    // Amt resources demanded by the sender or amt scale_up demanded of self.
    long int offering;   // Amt resources given by the sender
    // Maybe add other things here...
} message;

typedef struct {
    // Add state variables here for the LPs
    long int health;
    long int resources;
    long int offense;
    long int size;
    long int at_war_with; // gid of the other entity (-1 if not at war)
    long unsigned int health_lim;    // Upper health limit (NOT TO BE CONFUSED WITH SIMILARLY NAMED LOWER HEALTH LIMIT DEFINED IN THE .c FILE.
    long unsigned int times_defeated;
    long unsigned int times_won;
    long unsigned int wars_started;
    
} state;


typedef struct {
    long int health;
    long int health_lim;
    long int resources;
    long int offense;
    long int size;
    long int at_war_with;
    long int times_won;
    long int times_defeated;
    long int wars_started;
} final_stats;

/*
 * VERSUS Globals
 */
// NOTE: The following was copied from the phold model and adapted
tw_stime lookahead = 1.0;
static unsigned int stagger = 0;
static unsigned int offset_lpid = 0;
static tw_stime mult = 1.4;
static tw_stime percent_remote = 0.25;
static unsigned int ttl_lps = 0;
int nlp_per_pe = -1; // This gets reset by #define in .c file.
static int g_phold_start_events = 1;
static int optimistic_memory = 1000;

// rate for timestamp exponential distribution
static tw_stime mean = 1.0;

static char run_id[1024] = "undefined";

extern tw_lptype model_lps[];



#endif /* defined(____myModel3__) */
