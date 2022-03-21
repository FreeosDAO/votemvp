//#pragma once
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include "votemvp.hpp"
#include "tables.hpp"

using namespace eosio;
using namespace freedao;
using namespace std;

/*
void freeosgov::ratify_init() {
    ratify_index ratify_table(get_self(), get_self().value);
    auto ratify_iterator = ratify_table.begin();

    if(ratify_iterator == ratify_table.end()) {
        // emplace
        ratify_table.emplace(get_self(), [&](auto &r) { r.iteration = current_iteration(); });
    }
}

// reset the ratify record, ready for the new iteration
void freeosgov::ratify_reset() {
    ratify_index ratify_table(get_self(), get_self().value);
    auto ratify_iterator = ratify_table.begin();

    if (ratify_iterator != ratify_table.end()) {
        ratify_table.modify(ratify_iterator, get_self(), [&](auto &ratify) {
            ratify.iteration = current_iteration();
            ratify.participants = 0;
            ratify.ratified = 0;
        });
    }
}

// ACTION
void freeosgov::ratify(name user, bool ratify_vote) {
    
    require_auth(user);

    tick();

    // is the user registered?
    check(is_registered(user), "ratify is not open to unregistered users");
    
    // is the user verified?
    check(is_user_verified(user), "ratify is not open to unverified users");
    
    // is the system operational?
    uint32_t this_iteration = current_iteration();
    check(this_iteration != 0, "The freeos system is not available at this time");

    // is the user alive?
    check(is_user_alive(user), "user has exceeded the maximum number of iterations");

    // are we in the ratify period?
    check(is_action_period("ratify"), "It is outside of the ratify period");

    // has the user met the requirement of voting then ratifying?
    svr_index svrs_table(get_self(), user.value);
    auto svr_iterator = svrs_table.begin();
    check(svr_iterator != svrs_table.end(), "user must have voted in order to ratify");

    // check if the user has voted - a requirement for ratification
    check(svr_iterator->vote0 == this_iteration ||
        svr_iterator->vote1 == this_iteration ||
        svr_iterator->vote2 == this_iteration ||
        svr_iterator->vote3 == this_iteration ||
        svr_iterator->vote4 == this_iteration,
        "user must have voted in order to ratify");

    // check if the user has already ratified
    check(svr_iterator->ratify0 != this_iteration &&
        svr_iterator->ratify1 != this_iteration &&
        svr_iterator->ratify2 != this_iteration &&
        svr_iterator->ratify3 != this_iteration &&
        svr_iterator->ratify4 != this_iteration,
        "user has already ratified");


    // store the responses
    ratify_index ratify_table(get_self(), get_self().value);
    auto ratify_iterator = ratify_table.begin();
    check(ratify_iterator != ratify_table.end(), "ratify record is not defined");

    // process the responses from the user
    ratify_table.modify(ratify_iterator, get_self(), [&](auto &ratify) {

        // ratified?
        if (ratify_vote == true) {
            ratify.ratified++;
        }        

        // update the number of participants
        ratify.participants += 1;

    }); // end of modify
    
    // record that the user has ratified
    svrs_table.modify(svr_iterator, get_self(), [&](auto &svr) {
        switch (this_iteration % 5) {
            case 0: svr.ratify0 = this_iteration; break;
            case 1: svr.ratify1 = this_iteration; break;
            case 2: svr.ratify2 = this_iteration; break;
            case 3: svr.ratify3 = this_iteration; break;
            case 4: svr.ratify4 = this_iteration; break;
        }
    }); // end of modify

} */