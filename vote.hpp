//#pragma once
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include "votemvp.hpp"
#include "tables.hpp"
#include "config.hpp"

using namespace eosio;
using namespace freedao;
using namespace std;

void freeosgov::vote_init() {
    vote_index vote_table(get_self(), get_self().value);
    auto vote_iterator = vote_table.begin();

    if (vote_iterator == vote_table.end()) {
        // emplace
        vote_table.emplace(get_self(), [&](auto &v) { v.iteration = current_iteration(); });
    }
}

// rest the vote record, ready for the new iteration
void freeosgov::vote_reset() {
    vote_index vote_table(get_self(), get_self().value);
    auto vote_iterator = vote_table.begin();

    if (vote_iterator != vote_table.end()) {
        vote_table.modify(vote_iterator, get_self(), [&](auto &vote) {
            vote.iteration = current_iteration();
            vote.participants = 0;
            vote.q1average = 0.0;
            vote.q2average = 0.0;
            vote.q3average = 0.0;
            vote.q4choice1 = 0;   // POOL
            vote.q4choice2 = 0;   // BURN
            vote.q5average = 0.0;
            vote.q6choice1 = 0;
            vote.q6choice2 = 0;
            vote.q6choice3 = 0;
            vote.q6choice4 = 0;
            vote.q6choice5 = 0;
            vote.q6choice6 = 0;
        });
    }

}


std::vector<int> parse_vote_ranges(string voteranges) {
    
    // the voteranges string looks like this: q1:0-100,q2:6-30,q5:0-50
    std::vector<int> limits;

    std::vector<std::string> tokenlist = split(voteranges, ",");

    std::vector q1_param = split(tokenlist[0], ":");
    std::vector q2_param = split(tokenlist[1], ":");
    std::vector q5_param = split(tokenlist[2], ":");

    std::vector q1_minmax = split(q1_param[1], "-");
    std::vector q2_minmax = split(q2_param[1], "-");
    std::vector q5_minmax = split(q5_param[1], "-");

    limits.push_back (stoi(q1_minmax[0]));
    limits.push_back (stoi(q1_minmax[1]));
    limits.push_back (stoi(q2_minmax[0]));
    limits.push_back (stoi(q2_minmax[1]));
    limits.push_back (stoi(q5_minmax[0]));
    limits.push_back (stoi(q5_minmax[1]));

    return limits;
}

// ACTION
void freeosgov::vote(name user, double q3response) {
    
    // TODO: rewrite for reduced voting questions

    require_auth(user);

    tick();

    // is the user staked?
    check(is_staked(user), "voting is not open to unstaked users");
    
    // is the system operational?
    uint32_t this_iteration = current_iteration();
    check(this_iteration != 0, "the freeos system is not available at this time");

    // are we in the vote period?
    // check(is_action_period("vote"), "it is outside of the vote period");

    // has the user already completed the vote?
    svr_index svrs_table(get_self(), user.value);
    auto svr_iterator = svrs_table.begin();

    // if there is no svr record for the user then create it - we will update it at the end of the action
    if (svr_iterator == svrs_table.end()) {
        // emplace
        svrs_table.emplace(get_self(), [&](auto &svr) { ; });
        svr_iterator = svrs_table.begin();
    } else {
        check(svr_iterator->vote0 != this_iteration &&
            svr_iterator->vote1 != this_iteration &&
            svr_iterator->vote2 != this_iteration &&
            svr_iterator->vote3 != this_iteration &&
            svr_iterator->vote4 != this_iteration,
            "user has already voted");
    }

    // parameter checking
    // n.b. split function is defined in survey.hpp

    // get and parse the vote slider ranges
    // string voteranges = get_parameter(name("voteranges"));
    // std::vector<int> vote_range_values = parse_vote_ranges(voteranges);

    // get the current price of Freeos
    // get the configuration contract
    name config_account = name(get_parameter(name("configacct")));

    exchange_index rates_table(config_account, config_account.value);
    auto rate_iterator = rates_table.begin();
    check(rate_iterator != rates_table.end(), "current price of Freeos is undefined");
    double current_price = rate_iterator->currentprice;
    // double target_price = rate_iterator->targetprice;

    // calculate the upper bound of locking threshold (q3)
    double lock_factor = get_dparameter(name("lockfactor"));
    double locking_threshold_upper_limit = 0.0;
    if (current_price < HARD_EXCHANGE_RATE_FLOOR) {
        locking_threshold_upper_limit = lock_factor * HARD_EXCHANGE_RATE_FLOOR;
    } else {
        locking_threshold_upper_limit = lock_factor * current_price;
    }

    // argument validation
    std::string assert_message = "response 3 is out of range (" + to_string(HARD_EXCHANGE_RATE_FLOOR) + " - " + to_string(locking_threshold_upper_limit) + ")";
    check(q3response >= HARD_EXCHANGE_RATE_FLOOR && q3response <= locking_threshold_upper_limit,   assert_message);
    
    // store the responses
    vote_index vote_table(get_self(), get_self().value);
    auto vote_iterator = vote_table.begin();
    check(vote_iterator != vote_table.end(), "vote record is not defined");

    // process the responses from the user
    // for multiple choice options, increment to add the user's selection
    // for running averages, compute new running average
    vote_table.modify(vote_iterator, get_self(), [&](auto &vote) {

        // question 3
        vote.q3average = ((vote.q3average * vote.participants) + q3response) / (vote.participants + 1);

        // update the number of participants
        vote.participants += 1;

    }); // end of modify

    // record that the user has responded to this iteration's vote
    uint32_t survey_completed = 0;
    svrs_table.modify(svr_iterator, get_self(), [&](auto &svr) {
        switch (this_iteration % 5) {
            case 0: svr.vote0 = this_iteration; survey_completed = svr.survey0; break;
            case 1: svr.vote1 = this_iteration; survey_completed = svr.survey1; break;
            case 2: svr.vote2 = this_iteration; survey_completed = svr.survey2; break;
            case 3: svr.vote3 = this_iteration; survey_completed = svr.survey3; break;
            case 4: svr.vote4 = this_iteration; survey_completed = svr.survey4; break;
        }
    }); // end of modify

    // increment the number of participants in this iteration...
    // ... unless they have completed survey, in which case they have already been counted
    if (survey_completed != this_iteration) {
        // increment the number of participants in this iteration
        system_index system_table(get_self(), get_self().value);
        auto system_iterator = system_table.begin();
        check(system_iterator != system_table.end(), "system record is undefined");
        system_table.modify(system_iterator, get_self(), [&](auto &s) {
            s.participants += 1;
        });
    }

}