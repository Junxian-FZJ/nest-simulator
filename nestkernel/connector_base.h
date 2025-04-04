/*
 *  connector_base.h
 *
 *  This file is part of NEST.
 *
 *  Copyright (C) 2004 The NEST Initiative
 *
 *  NEST is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  NEST is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with NEST.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef CONNECTOR_BASE_H
#define CONNECTOR_BASE_H

// Generated includes:
#include "config.h"

// C++ includes:
#include <cstdlib>
#include <vector>

// Includes from libnestutil:
#include "compose.hpp"
#include "sort.h"
#include "vector_util.h"

// Includes from nestkernel:
#include "common_synapse_properties.h"
#include "connection_label.h"
#include "connector_model.h"
#include "event.h"
#include "nest_datums.h"
#include "nest_names.h"
#include "node.h"
#include "source.h"
#include "spikecounter.h"

// Includes from sli:
#include "arraydatum.h"
#include "dictutils.h"

namespace nest
{

/**
 * Base class to allow storing Connectors for different synapse types
 * in vectors. We define the interface here to avoid casting.
 *
 * @note If any member functions need to do something special for a given connection type,
 * declare specializations in the corresponding header file and define them in the corresponding
 * source file. For an example, see `eprop_synapse_bsshslm_2020`.
 */
class ConnectorBase
{

public:
  // Destructor needs to be declared virtual to avoid undefined
  // behavior, avoid possible memory leak and needs to be defined to
  // avoid linker error, see, e.g., Meyers, S. (2005) p40ff
  virtual ~ConnectorBase() {};

  /**
   * Return syn_id_ of the synapse type of this Connector (index in
   * list of synapse prototypes).
   */
  virtual synindex get_syn_id() const = 0;

  /**
   * Return the number of connections in this Connector.
   */
  virtual size_t size() const = 0;

  /**
   * Write status of the connection at position lcid to the dictionary
   * dict.
   */
  virtual void get_synapse_status( const size_t tid, const size_t lcid, DictionaryDatum& dict ) const = 0;

  /**
   * Set status of the connection at position lcid according to the
   * dictionary dict.
   */
  virtual void set_synapse_status( const size_t lcid, const DictionaryDatum& dict, ConnectorModel& cm ) = 0;

  /**
   * Add ConnectionID with given source_node_id and lcid to conns. If
   * target_node_id is given, only add connection if target_node_id matches
   * the node_id of the target of the connection.
   */
  virtual void get_connection( const size_t source_node_id,
    const size_t target_node_id,
    const size_t tid,
    const size_t lcid,
    const long synapse_label,
    std::deque< ConnectionID >& conns ) const = 0;

  /**
   * Add ConnectionID with given source_node_id and lcid to conns. If
   * target_neuron_node_ids is given, only add connection if
   * target_neuron_node_ids contains the node ID of the target of the connection.
   */
  virtual void get_connection_with_specified_targets( const size_t source_node_id,
    const std::vector< size_t >& target_neuron_node_ids,
    const size_t tid,
    const size_t lcid,
    const long synapse_label,
    std::deque< ConnectionID >& conns ) const = 0;

  /**
   * Add ConnectionIDs with given source_node_id to conns, looping over
   * all lcids. If target_node_id is given, only add connection if
   * target_node_id matches the node ID of the target of the connection.
   */
  virtual void get_all_connections( const size_t source_node_id,
    const size_t target_node_id,
    const size_t tid,
    const long synapse_label,
    std::deque< ConnectionID >& conns ) const = 0;

  /**
   * For a given target_node_id add lcids of all connections with matching
   * node ID of target to source_lcids.
   */
  virtual void
  get_source_lcids( const size_t tid, const size_t target_node_id, std::vector< size_t >& source_lcids ) const = 0;

  /**
   * For a given start_lcid add node IDs of all targets that belong to the
   * same source to target_node_ids.
   */
  virtual void get_target_node_ids( const size_t tid,
    const size_t start_lcid,
    const std::string& post_synaptic_element,
    std::vector< size_t >& target_node_ids ) const = 0;

  /**
   * For a given lcid return the node ID of the target of the connection.
   */
  virtual size_t get_target_node_id( const size_t tid, const unsigned int lcid ) const = 0;

  /**
   * Send the event e to all connections of this Connector.
   */
  virtual void send_to_all( const size_t tid, const std::vector< ConnectorModel* >& cm, Event& e ) = 0;

  /**
   * Send the event e to the connection at position lcid. Return bool
   * indicating whether the following connection belongs to the same
   * source.
   */
  virtual size_t send( const size_t tid, const size_t lcid, const std::vector< ConnectorModel* >& cm, Event& e ) = 0;

  virtual void
  send_weight_event( const size_t tid, const unsigned int lcid, Event& e, const CommonSynapseProperties& cp ) = 0;

  /**
   * Update weights of dopamine modulated STDP connections.
   */
  virtual void trigger_update_weight( const long vt_node_id,
    const size_t tid,
    const std::vector< spikecounter >& dopa_spikes,
    const double t_trig,
    const std::vector< ConnectorModel* >& cm ) = 0;

  /**
   * Sort connections according to source node IDs.
   */
  virtual void sort_connections( BlockVector< Source >& ) = 0;

  /**
   * Set a flag in the connection indicating whether the following
   * connection belongs to the same source.
   */
  virtual void set_source_has_more_targets( const size_t lcid, const bool has_more_targets ) = 0;

  /**
   * Return lcid of the first connection after start_lcid (inclusive)
   * where the node_id of the target matches target_node_id. If there are no matches,
   * the function returns invalid_index.
   */
  virtual size_t find_first_target( const size_t tid, const size_t start_lcid, const size_t target_node_id ) const = 0;

  /**
   * Return lcid of first connection where the node ID of the target
   * matches target_node_id; consider only the connections with lcids
   * given in matching_lcids. If there is no match, the function returns
   * invalid_index.
   */
  virtual size_t find_matching_target( const size_t tid,
    const std::vector< size_t >& matching_lcids,
    const size_t target_node_id ) const = 0;

  /**
   * Disable the transfer of events through the connection at position
   * lcid.
   */
  virtual void disable_connection( const size_t lcid ) = 0;

  /**
   * Remove disabled connections from the connector.
   */
  virtual void remove_disabled_connections( const size_t first_disabled_index ) = 0;

#ifdef HAVE_SIONLIB
  virtual void dump_connections( const int sionlib_file_id,
    const size_t chunk_size,
    const size_t chunk_size_left,
    const size_t tid ) = 0;
#else
  virtual void dump_connections( std::ostream& connections_out, const size_t tid ) = 0;
#endif
};

/**
 * Homogeneous connector, contains synapses of one particular type (syn_id_).
 */
template < typename ConnectionT >
class Connector : public ConnectorBase
{
private:
  BlockVector< ConnectionT > C_;
  const synindex syn_id_;

public:
  explicit Connector( const synindex syn_id )
    : syn_id_( syn_id )
  {
  }

  ~Connector() override
  {
    C_.clear();
  }

  synindex
  get_syn_id() const override
  {
    return syn_id_;
  }

  size_t
  size() const override
  {
    return C_.size();
  }

  void
  get_synapse_status( const size_t tid, const size_t lcid, DictionaryDatum& dict ) const override
  {
    assert( lcid < C_.size() );

    C_[ lcid ].get_status( dict );

    // get target node ID here, where tid is available
    // necessary for hpc synapses using TargetIdentifierIndex
    def< long >( dict, names::target, C_[ lcid ].get_target( tid )->get_node_id() );
  }

  void
  set_synapse_status( const size_t lcid, const DictionaryDatum& dict, ConnectorModel& cm ) override
  {
    assert( lcid < C_.size() );

    C_[ lcid ].set_status( dict, static_cast< GenericConnectorModel< ConnectionT >& >( cm ) );
  }

  void
  push_back( const ConnectionT& c )
  {
    C_.push_back( c );
  }

  void
  push_back( ConnectionT&& c )
  {
    C_.push_back( std::move( c ) );
  }

  void
  get_connection( const size_t source_node_id,
    const size_t target_node_id,
    const size_t tid,
    const size_t lcid,
    const long synapse_label,
    std::deque< ConnectionID >& conns ) const override
  {
    if ( not C_[ lcid ].is_disabled() )
    {
      if ( synapse_label == UNLABELED_CONNECTION or C_[ lcid ].get_label() == synapse_label )
      {
        const size_t current_target_node_id = C_[ lcid ].get_target( tid )->get_node_id();
        if ( current_target_node_id == target_node_id or target_node_id == 0 )
        {
          conns.push_back(
            ConnectionDatum( ConnectionID( source_node_id, current_target_node_id, tid, syn_id_, lcid ) ) );
        }
      }
    }
  }

  void
  get_connection_with_specified_targets( const size_t source_node_id,
    const std::vector< size_t >& target_neuron_node_ids,
    const size_t tid,
    const size_t lcid,
    const long synapse_label,
    std::deque< ConnectionID >& conns ) const override
  {
    if ( not C_[ lcid ].is_disabled() )
    {
      if ( synapse_label == UNLABELED_CONNECTION or C_[ lcid ].get_label() == synapse_label )
      {
        const size_t current_target_node_id = C_[ lcid ].get_target( tid )->get_node_id();
        if ( std::find( target_neuron_node_ids.begin(), target_neuron_node_ids.end(), current_target_node_id )
          != target_neuron_node_ids.end() )
        {
          conns.push_back(
            ConnectionDatum( ConnectionID( source_node_id, current_target_node_id, tid, syn_id_, lcid ) ) );
        }
      }
    }
  }

  void
  get_all_connections( const size_t source_node_id,
    const size_t target_node_id,
    const size_t tid,
    const long synapse_label,
    std::deque< ConnectionID >& conns ) const override
  {
    for ( size_t lcid = 0; lcid < C_.size(); ++lcid )
    {
      get_connection( source_node_id, target_node_id, tid, lcid, synapse_label, conns );
    }
  }

  void
  get_source_lcids( const size_t tid, const size_t target_node_id, std::vector< size_t >& source_lcids ) const override
  {
    for ( size_t lcid = 0; lcid < C_.size(); ++lcid )
    {
      const size_t current_target_node_id = C_[ lcid ].get_target( tid )->get_node_id();
      if ( current_target_node_id == target_node_id and not C_[ lcid ].is_disabled() )
      {
        source_lcids.push_back( lcid );
      }
    }
  }

  void
  get_target_node_ids( const size_t tid,
    const size_t start_lcid,
    const std::string& post_synaptic_element,
    std::vector< size_t >& target_node_ids ) const override
  {
    size_t lcid = start_lcid;
    while ( true )
    {
      if ( C_[ lcid ].get_target( tid )->get_synaptic_elements( post_synaptic_element ) != 0.0
        and not C_[ lcid ].is_disabled() )
      {
        target_node_ids.push_back( C_[ lcid ].get_target( tid )->get_node_id() );
      }

      if ( not C_[ lcid ].source_has_more_targets() )
      {
        break;
      }

      ++lcid;
    }
  }

  size_t
  get_target_node_id( const size_t tid, const unsigned int lcid ) const override
  {
    return C_[ lcid ].get_target( tid )->get_node_id();
  }

  void
  send_to_all( const size_t tid, const std::vector< ConnectorModel* >& cm, Event& e ) override
  {
    auto const& cp = static_cast< GenericConnectorModel< ConnectionT >* >( cm[ syn_id_ ] )->get_common_properties();

    for ( size_t lcid = 0; lcid < C_.size(); ++lcid )
    {
      e.set_port( lcid );
      assert( not C_[ lcid ].is_disabled() );
      C_[ lcid ].send( e, tid, cp );
    }
  }

  size_t
  send( const size_t tid, const size_t lcid, const std::vector< ConnectorModel* >& cm, Event& e ) override
  {
    typename ConnectionT::CommonPropertiesType const& cp =
      static_cast< GenericConnectorModel< ConnectionT >* >( cm[ syn_id_ ] )->get_common_properties();

    size_t lcid_offset = 0;

    while ( true )
    {
      assert( lcid + lcid_offset < C_.size() );
      ConnectionT& conn = C_[ lcid + lcid_offset ];

      e.set_port( lcid + lcid_offset );
      if ( not conn.is_disabled() )
      {
        // Some synapses, e.g., bernoulli_synapse, may not send an event after all
        const bool event_sent = conn.send( e, tid, cp );
        if ( event_sent )
        {
          send_weight_event( tid, lcid + lcid_offset, e, cp );
        }
      }
      if ( not conn.source_has_more_targets() )
      {
        break;
      }
      ++lcid_offset;
    }

    return 1 + lcid_offset; // event was delivered to at least one target
  }

  // Implemented in connector_base_impl.h
  void
  send_weight_event( const size_t tid, const unsigned int lcid, Event& e, const CommonSynapseProperties& cp ) override;

  void
  trigger_update_weight( const long vt_node_id,
    const size_t tid,
    const std::vector< spikecounter >& dopa_spikes,
    const double t_trig,
    const std::vector< ConnectorModel* >& cm ) override
  {
    for ( size_t i = 0; i < C_.size(); ++i )
    {
      if ( static_cast< GenericConnectorModel< ConnectionT >* >( cm[ syn_id_ ] )
             ->get_common_properties()
             .get_vt_node_id()
        == vt_node_id )
      {
        C_[ i ].trigger_update_weight( tid,
          dopa_spikes,
          t_trig,
          static_cast< GenericConnectorModel< ConnectionT >* >( cm[ syn_id_ ] )->get_common_properties() );
      }
    }
  }

  void
  sort_connections( BlockVector< Source >& sources ) override
  {
    nest::sort( sources, C_ );
  }

  void
  set_source_has_more_targets( const size_t lcid, const bool has_more_targets ) override
  {
    C_[ lcid ].set_source_has_more_targets( has_more_targets );
  }

  size_t
  find_first_target( const size_t tid, const size_t start_lcid, const size_t target_node_id ) const override
  {
    size_t lcid = start_lcid;
    while ( true )
    {
      if ( C_[ lcid ].get_target( tid )->get_node_id() == target_node_id and not C_[ lcid ].is_disabled() )
      {
        return lcid;
      }

      if ( not C_[ lcid ].source_has_more_targets() )
      {
        return invalid_index;
      }

      ++lcid;
    }
  }

  size_t
  find_matching_target( const size_t tid,
    const std::vector< size_t >& matching_lcids,
    const size_t target_node_id ) const override
  {
    for ( size_t i = 0; i < matching_lcids.size(); ++i )
    {
      if ( C_[ matching_lcids[ i ] ].get_target( tid )->get_node_id() == target_node_id )
      {
        return matching_lcids[ i ];
      }
    }

    return invalid_index;
  }

  void
  disable_connection( const size_t lcid ) override
  {
    assert( not C_[ lcid ].is_disabled() );
    C_[ lcid ].disable();
  }

  void
  remove_disabled_connections( const size_t first_disabled_index ) override
  {
    assert( C_[ first_disabled_index ].is_disabled() );
    C_.erase( C_.begin() + first_disabled_index, C_.end() );
  }

#ifdef HAVE_SIONLIB
  void dump_connections( const int sionlib_file_id,
    const size_t chunk_size,
    const size_t chunk_size_left,
    const size_t tid ) override;
#else
  void dump_connections( std::ostream& connections_out, const size_t tid ) override;
#endif
};

} // of namespace nest

#endif
