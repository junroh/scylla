/*
 * Copyright (C) 2020 ScyllaDB
 */

/*
 * This file is part of Scylla.
 *
 * Scylla is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Scylla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scylla.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <seastar/core/condition-variable.hh>
#include "raft.hh"
#include "logical_clock.hh"

namespace raft {

// Leader's view of each follower, including self.
class follower_progress {
public:
    // Id of this server
    server_id id;
    // Index of the next log entry to send to this server.
    index_t next_idx;
    // Index of the highest log entry known to be replicated to this
    // server.
    index_t match_idx = index_t(0);
    // Index that we know to be committed by the follower
    index_t commit_idx = index_t(0);

    enum class state {
        // In this state only one append entry is send until matching index is found
        PROBE,
        // In this state multiple append entries are sent optimistically
        PIPELINE,
        // In this state snapshot is been transfered
        SNAPSHOT
    };
    state state = state::PROBE;
    // true if a packet was sent already in a probe mode
    bool probe_sent = false;
    // number of in flight still un-acked append entries requests
    size_t in_flight = 0;
    static constexpr size_t max_in_flight = 10;

    // check if a reject packet should be ignored because it was delayed
    // or reordered
    bool is_stray_reject(const append_reply::rejected&);

    void become_probe();
    void become_pipeline();
    void become_snapshot();

    // Return true if a new replication record can be sent to the follower.
    bool can_send_to();

    follower_progress(server_id id_arg, index_t next_idx_arg)
        : id(id_arg), next_idx(next_idx_arg)
    {}
};

using progress = std::unordered_map<server_id, follower_progress>;

class tracker: private progress {
    // Copy of this server's id
    server_id _my_id;
    configuration _configuration;
    // Not NULL if the leader is part of the current configuration.
    //
    // 4.2.2 Removing the current leader
    // There will be a period of time (while it is committing
    // C_new) when a leader can manage a cluster that does not
    // include itself; it replicates log entries but does not
    // count itself in majorities.
    follower_progress *_leader_progress = nullptr;
public:
    using progress::begin, progress::end, progress::cbegin, progress::cend, progress::size;

    explicit tracker(server_id my_id)
            : _my_id(my_id)
    {}

    // Return progress for a follower
    follower_progress& find(server_id dst) {
        return this->progress::find(dst)->second;
    }
    void set_configuration(configuration configuration, index_t next_idx);
    // Return progress object for the current leader if it's
    // part of the current configuration.
    follower_progress* leader_progress() {
        return _leader_progress;
    }
    const configuration& get_configuration() const { return _configuration; }

    // Calculate the current commit index based on the current
    // simple or joint quorum.
    index_t committed(index_t prev_commit_idx);
};

// Possible leader election outcomes.
enum class vote_result {
    // We haven't got enough responses yet, either because
    // the servers haven't voted or responses failed to arrive.
    UNKNOWN,
    // This candidate has won the election
    WON,
    // The quorum of servers has voted against this candidate
    LOST,
};

// State of election in a single quorum
class election_tracker {
    size_t _responded = 0;
    size_t _granted = 0;
public:
    void register_vote(bool granted) {
        _responded++;
        if (granted) {
            _granted++;
        }
    }
    vote_result tally_votes(size_t cluster_size) const {
        auto quorum = cluster_size / 2 + 1;
        if (_granted >= quorum) {
            return vote_result::WON;
        }
        assert(_responded <= cluster_size);
        auto unknown = cluster_size - _responded;
        return _granted + unknown >= quorum ? vote_result::UNKNOWN : vote_result::LOST;
    }
    friend std::ostream& operator<<(std::ostream& os, const election_tracker& v);
};

// Candidate's state specific to election
class votes {
    configuration _configuration;
    server_address_set _voters;
    election_tracker _current;
    election_tracker _previous;
public:
    const server_address_set& voters() const {
        return _voters;
    }
    void set_configuration(configuration configuration);

    void register_vote(server_id from, bool granted);
    vote_result tally_votes() const;

    const configuration& get_configuration() const {
        return _configuration;
    }

    friend std::ostream& operator<<(std::ostream& os, const votes& v);
};

} // namespace raft

