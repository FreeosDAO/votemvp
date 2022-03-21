#include "votemvp.hpp"
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include "tables.hpp"
#include "config.hpp"
#include "identity.hpp"
#include "survey.hpp"
#include "vote.hpp"
#include "ratify.hpp"
#include "points.hpp"
#include "claim.hpp"
#include "maintain.hpp"

namespace freedao {

using namespace eosio;
using namespace std;

const std::string VERSION = "0.1.3mvp";

// ACTION
void freeosgov::version() {

  string version_message = "Version = " + VERSION + ", Iteration = " + to_string(current_iteration());

  check(false, version_message);
}

// ACTION
void freeosgov::init() {

  require_auth(get_self());

  system_index system_table(get_self(), get_self().value);
  auto system_iterator = system_table.begin();
  if (system_iterator == system_table.end()) {
    // insert system record
    system_table.emplace(get_self(), [&](auto &sys) {
      sys.init = current_time_point();
      sys.cls = asset(0, POINT_CURRENCY_SYMBOL);
      });
  } else {
    // modify system record
    system_table.modify(system_iterator, get_self(), [&](auto &sys) { sys.init = current_time_point(); });
  }

  // create the survey, vote and ratify records if they don't already exist
  // survey
  // survey_init();

  // vote
  vote_init();

  // ratify
  // ratify_init();

}


// helper functions

// are we in the survey, vote or ratify period?
bool freeosgov::is_action_period(string action) {
  // default return is false
  bool result = false;

  string action_parameter_start = action + "start";
  string action_parameter_end = action + "end";
  string err_msg_start = action_parameter_start + " is undefined";
  string err_msg_end = action_parameter_end + " is undefined";

  // get the start of freeos system time
  system_index system_table(get_self(), get_self().value);
  auto system_iterator = system_table.begin();
  check(system_iterator != system_table.end(), "system record is undefined");
  time_point init = system_iterator->init;

  // how far are we into the current iteration?
  uint64_t now_secs = current_time_point().sec_since_epoch();
  uint64_t init_secs = init.sec_since_epoch();
  uint32_t iteration_secs = (now_secs - init_secs) % ITERATION_LENGTH_SECONDS;

  // get the config parameters for surveystart and surveyend
  parameters_index parameters_table(get_self(), get_self().value);

  auto parameter_iterator = parameters_table.find(name(action_parameter_start).value);
  check(parameter_iterator != parameters_table.end(), err_msg_start);
  uint32_t action_start = stoi(parameter_iterator->value);

  parameter_iterator = parameters_table.find(name(action_parameter_end).value);
  check(parameter_iterator != parameters_table.end(), err_msg_end);
  uint32_t action_end = stoi(parameter_iterator->value);
  
  if (iteration_secs >= action_start && iteration_secs <= action_end) {
    result = true;
  }

  /* string debug_msg = "now_secs=" + to_string(now_secs) + ", init_secs=" + to_string(init_secs) +
   ", iteration_secs=" + to_string(iteration_secs) + ", surveystart=" + to_string(surveystart) + ", surveyend=" + to_string(surveyend);
  check(false, debug_msg); */

  return result;
}


/* which iteration are we in?
uint16_t freeosgov::current_iteration() {

  uint16_t iteration = 0;

  // get the start of freeos system time
  system_index system_table(get_self(), get_self().value);
  auto system_iterator = system_table.begin();
  check(system_iterator != system_table.end(), "system record is undefined");
  time_point init = system_iterator->init;

  // how far are we into the current iteration?
  uint64_t now_secs = current_time_point().sec_since_epoch();
  uint64_t init_secs = init.sec_since_epoch();

  if (now_secs >= init_secs) {
    iteration = ((now_secs - init_secs) / ITERATION_LENGTH_SECONDS) + 1;
  }
  
  return iteration;
} */

// AirClaim-style iteration calculation
// return the current iteration record
uint32_t freeosgov::current_iteration() {
  uint32_t this_iteration = 0;

  uint64_t now = current_time_point().time_since_epoch()._count;

  name config_account = name(get_parameter(name("configacct")));

  // find iteration that matches current time
  iterations_index iterations_table(config_account,
                                    config_account.value);
  auto start_index = iterations_table.get_index<"start"_n>();
  auto iteration_iterator = start_index.upper_bound(now);

  if (iteration_iterator != start_index.begin()) {
    iteration_iterator--;
  }

  // check we are within the period of the iteration
  if (iteration_iterator != start_index.end() &&
      now >= iteration_iterator->start.time_since_epoch()._count &&
      now <= iteration_iterator->end.time_since_epoch()._count) {
    this_iteration = iteration_iterator->iteration_number;
  }

  return this_iteration;
}




/* ACTION
void freeosgov::tick() {

  // are we on a new iteration?
  // system_index system_table(get_self(), get_self().value);
  // auto system_iterator = system_table.begin();
  // check(system_iterator != system_table.end(), "system record is undefined");

  uint16_t recorded_iteration = system_iterator->iteration;
  uint16_t this_iteration = current_iteration();

  // set the new iteration in the system record
  if (this_iteration != recorded_iteration) {
    // run the new iteration change service routine
    trigger_new_iteration(this_iteration);

    // update the recorded iteration
    system_table.modify(system_iterator, get_self(), [&](auto &sys) {
      sys.iteration = this_iteration;
      sys.participants = 0;
    });

  }
} */


// ACTION
void freeosgov::tick() {

  // what iteration is in the system table?
  system_index system_table(get_self(), get_self().value);
  auto system_iterator = system_table.begin();
  check(system_iterator != system_table.end(),
        "system record is not found");

  uint32_t old_iteration = system_iterator->iteration;
  uint32_t new_iteration = current_iteration();

  if (new_iteration != old_iteration) {
    // a change in iteration has occurred
    trigger_new_iteration(new_iteration);

    // write the new iteration value back to the statistics record
    system_table.modify(system_iterator, get_self(), [&](auto &sys) {
      sys.iteration = new_iteration;
    });
  }

  
}

// tidy up at the end of an iteration - save SVR data in the reward record
void freeosgov::trigger_new_iteration(uint32_t new_iteration) {

  // if the transition is from iteration 0 to 1 there is nothing to do, so return
  if (new_iteration == 1) return;

  // record/update the old and new iterations - and take snapshot of the CLS at this point
  system_index system_table(get_self(), get_self().value);
  auto system_iterator = system_table.begin();
  check(system_iterator != system_table.end(), "system record is undefined");

  // capture the data we need from the system, vote and ratify records
  // 1. system record
  uint32_t participants = system_iterator->participants;
  uint32_t old_iteration = system_iterator->iteration;

  // 2. vote record
  vote_index vote_table(get_self(), get_self().value);
  auto vote_iterator = vote_table.begin();
  check(vote_iterator != vote_table.end(), "vote record is undefined");
  // locking threshold
  double locking_threshold = vote_iterator->q3average;

  // find the locking threshold quorum
  std::string locking_quorum_str = get_parameter(name("lockquorum"));
  uint32_t  locking_quorum = stoi(locking_quorum_str);

  if (vote_iterator->participants >= locking_quorum) {
    // write the locking threshold back to the exchangerate table on freeoscfg
    action transfer_action = action(
        permission_level{get_self(), "active"_n}, name(freeosconfig_acct),
        "targetrate"_n,
        std::make_tuple(locking_threshold));

    transfer_action.send();
  }
  
  // reset the survey, vote and ratify records, ready for the new iteration
  vote_reset();

}

} // end of namespace freedao
