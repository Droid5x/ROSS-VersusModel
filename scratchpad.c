    // Onto Event Handling:
    switch (input_msg->type) {
        case DECLARE_WAR:               // We have just received a war declaration from someone.
            if (s->at_war_with == -1){  // We aren't at war yet with anyone else
                war_field = 0;          // Set the bit field to record this for the reverse handler
                fprintf(stderr, "NOTE: LP %llu just declared war on LP %llu.\n", input_msg->sender, lp->gid);
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
                        current_event = tw_event_new(lp->gid,timestamp + 2, lp);
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
                    fight_field = 0;
                    current_event = tw_event_new(input_msg->sender, timestamp + 1, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = SURRENDER;
                    new_message->sender = lp->gid;
                    // Pay as much of the demands as possible:
                    if (s->resources >= input_msg->demands){
                        offer_field = 1;
                        new_message->offering = input_msg->demands;
                        s->resources -= input_msg->demands;
                    }
                    else {
                        offer_field = 0;
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
                    fight_field = 1;
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
            int change;
            // Note that casting to int will operate identically to math's floor function for positive numbers
            // Add resources proportional to the size of the entity
            s->resources += (int)(s->size*tw_rand_unif(lp->rng)*RESOURCERATE);
            // This also ensures that the LP will keep simulating by always having an ADD_RESOURCES event to respond to
            // TODO: re-implement the reverse handler and necessary bit-field flags to fit this new event subhandler
            current_event = tw_event_new(lp->gid, timestamp + 1, lp);
            new_message = tw_event_data(current_event);
            new_message->type = ADD_RESOURCES;
            new_message->sender = lp->gid;
            tw_event_send(current_event);
            /*rebuild_field = 0;
            expand_field = 0;*/
            // Now we can do several of five things: scale up the arsenal, scale it down to recover resources,
            // rebuild (to regenerate health), declare war to get more resources if we aren't already
            // fighting, and expand our size/empire
            // All of these events will occur now, rather than being implemented as actual events to be
            // Received in the future
            if (s->health < HEALTH_LIM) {   // Our health is low, so begin repairs
                while (s->health < s->health_lim && s->resources > RESOURCE_LIM) {
                    // TODO: maybe add a limit to how much one can rebuild in one 'turn'
                    s->health ++;
                    s->resources --;
                }
            }
            if (s->at_war_with != -1) { // We are in a state of war, so arm up
                // Arm up if we can
                change = (int)(tw_rand_unif(lp->rng) * SCALE_AMT);
                s->offense += change;
                s->rescources -= change;
            }
            else if (s->resources < RESOURCE_LIM && s->offense > DONWSCALE_LIM) {
                // If we don't have enough resources, we have two choices aside from waiting:
                // Scale down and recover resources, or declare war to take someone else's
                if (s->health < HEALTH_LIM) {   // Downscale
                    change = (int)(tw_rand_unif(lp->rng) * SCALE_AMT);
                    s->offense -= change;
                    s->resources += change;
                }
                else {  // Declare War on a random LP
                    // Randomly choose an lp to attack (who isn't us)
                    int attack_gid;
                    // TODO: make sure that this allows the LP to attack LPs on other MPI nodes
                    attack_gid = tw_rand_integer(lp->rng,0,NUMLPS-2);
                    if (attack_gid == lp->gid)
                        attack_gid++;
                    s->at_war_with = attack_gid;
                    // Generate and send the declaration
                    current_event = tw_event_new(attack_gid, timestamp, lp);
                    new_message = tw_event_data(current_event);
                    new_message->type = DECLARE_WAR;
                    new_message->sender = lp->gid;
                    // TODO: design a more stochastic method of generating the demand amt!
                    new_message->demands = DEFAULT_DEMAND_AMT;
                    new_message->damage = s->offense;
                    tw_event_send(current_event);
                    s->wars_started ++;
                    fprintf(stderr, "NOTE: LP %llu sent a war declaration on LP %llu.\n", lp->gid, attack_gid);
                }
            }
            else () {   // We have nothing else to do, so expand the empire
                while (s->resources >= RESOURCE_LIM) {
                    // TODO refine this system to something better than just incremeting/decrementing
                    s->resources --;
                    s->size ++;
                    s->health_lim ++;
                }
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