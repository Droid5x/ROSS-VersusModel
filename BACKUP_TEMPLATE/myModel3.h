//
//  myModel3.h
//  
//  A test/toy model for ROSS (Rensselaer Optimistic Simulation System
//  Created by Mark P. Blanco on 6/8/15.
//
//

#ifndef ____myModel3__
#define ____myModel3__

#include <stdio.h>
#include "ross.h"

// Define message types here
typedef enum message_type {
    
};

// Define message data here:
typedef struct message {
    message_type type;
    // add other message contents here
};

typedef struct state {
    // Add state variables here for the LPs
};

extern tw_lptype model_lps[];



#endif /* defined(____myModel3__) */
