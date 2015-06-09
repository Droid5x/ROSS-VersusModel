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
#define field6 bf->c6
#define field7 bf->c7
#define field8 bf->c8
#define field9 bf->c9
#define field10 bf->c10
#define field11 bf->c11
#define field12 bf->c12
#define field13 bf->c13
#define field14 bf->c14
#define field15 bf->c15
#define field16 bf->c16
#define field17 bf->c17
#define field18 bf->c18
#define field19 bf->c19
#define field20 bf->c20
#define field21 bf->c21
#define field22 bf->c22
#define field23 bf->c23
#define field24 bf->c24
#define field25 bf->c25
#define field26 bf->c26
#define field27 bf->c27
#define field28 bf->c28
#define field29 bf->c29
#define field30 bf->c30
#define field31 bf->c31

// Define message types here
typedef enum {
    DECLARE_WAR,
    PROPOSE_PEACE,
    ACCEPT_PEACE,
    SCALE_UP,
    SCALE_DOWN,
    FIGHT,       //(Note that the defender always gets the first strike)
    REBUILD,
    EXPAND,
    ADD_RESOURCES
} message_type;

// Define message data here:
typedef struct {
    message_type type;
    tw_lpid sender;
    int damage;     // The damage caused by the sender
    int demands;    // Amt resources demanded by the sender or amt scale_up demanded of self.
    int offering;   // Amt resources given by the sender
    // Maybe add other things here...
} message;

typedef struct {
    // Add state variables here for the LPs
    int health;
    int resources;
    unsigned int offense;
    unsigned int size;
    int at_war_with; // gid of the other entity (-1 if not at war)
} state;

/*
 * VERSUS Globals
 */
// NOTE: Lines 52-65 were copied from the phold model and adapted
tw_stime lookahead = 1.0;
static unsigned int stagger = 0;
static unsigned int offset_lpid = 0;
static tw_stime mult = 1.4;
static tw_stime percent_remote = 0.25;
static unsigned int ttl_lps = 0;
static unsigned int nlp_per_pe = -1; // This gets reset by #define in .c file.
static int g_phold_start_events = 1;
static int optimistic_memory = 1000;

// rate for timestamp exponential distribution
static tw_stime mean = 1.0;

static char run_id[1024] = "undefined";

extern tw_lptype model_lps[];



#endif /* defined(____myModel3__) */
