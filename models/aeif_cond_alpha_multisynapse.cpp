/*
 *  aeif_cond_alpha_multisynapse.cpp
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

#include "aeif_cond_alpha_multisynapse.h"

#ifdef HAVE_GSL

// C++ includes:
#include <limits>

// Includes from libnestutil:
#include "dict_util.h"
#include "numerics.h"

// Includes from nestkernel:
#include "exceptions.h"
#include "kernel_manager.h"
#include "nest_impl.h"
#include "universal_data_logger_impl.h"

// Includes from sli:
#include "dict.h"
#include "dictutils.h"


namespace nest // template specialization must be placed in namespace
{
void
register_aeif_cond_alpha_multisynapse( const std::string& name )
{
  register_node_model< aeif_cond_alpha_multisynapse >( name );
}


/* ----------------------------------------------------------------
 * Recordables map
 * ---------------------------------------------------------------- */

// Override the create() method with one call to RecordablesMap::insert_()
// for each quantity to be recorded.
template <>
void
DynamicRecordablesMap< aeif_cond_alpha_multisynapse >::create( aeif_cond_alpha_multisynapse& host )
{
  // use standard names wherever you can for consistency!
  insert( names::V_m, host.get_data_access_functor( aeif_cond_alpha_multisynapse::State_::V_M ) );

  insert( names::w, host.get_data_access_functor( aeif_cond_alpha_multisynapse::State_::W ) );

  host.insert_conductance_recordables();
}

Name
aeif_cond_alpha_multisynapse::get_g_receptor_name( size_t receptor )
{
  std::stringstream receptor_name;
  receptor_name << "g_" << receptor + 1;
  return Name( receptor_name.str() );
}

void
aeif_cond_alpha_multisynapse::insert_conductance_recordables( size_t first )
{
  for ( size_t receptor = first; receptor < P_.E_rev.size(); ++receptor )
  {
    size_t elem = aeif_cond_alpha_multisynapse::State_::G
      + receptor * aeif_cond_alpha_multisynapse::State_::NUM_STATE_ELEMENTS_PER_RECEPTOR;
    recordablesMap_.insert( get_g_receptor_name( receptor ), this->get_data_access_functor( elem ) );
  }
}

DataAccessFunctor< aeif_cond_alpha_multisynapse >
aeif_cond_alpha_multisynapse::get_data_access_functor( size_t elem )
{
  return DataAccessFunctor< aeif_cond_alpha_multisynapse >( *this, elem );
}

/* ----------------------------------------------------------------
 * Right-hand side function
 * ---------------------------------------------------------------- */

extern "C" int
aeif_cond_alpha_multisynapse_dynamics( double, const double y[], double f[], void* pnode )
{
  // y[] is the state vector supplied by the integrator,
  // not the state vector in the node, node.S_.y[].

  typedef nest::aeif_cond_alpha_multisynapse::State_ S;

  // get access to node so we can almost work as in a member function
  assert( pnode );
  const nest::aeif_cond_alpha_multisynapse& node =
    *( reinterpret_cast< nest::aeif_cond_alpha_multisynapse* >( pnode ) );

  const bool is_refractory = node.S_.r_ > 0;

  // Clamp membrane potential to V_reset while refractory, otherwise bound
  // it to V_peak. Do not use V_.V_peak_ here, since that is set to V_th if
  // Delta_T == 0.
  const double& V = is_refractory ? node.P_.V_reset_ : std::min( y[ S::V_M ], node.P_.V_peak_ );
  const double& w = y[ S::W ];

  // I_syn = - sum_k g_k (V - E_rev_k).
  double I_syn = 0.0;
  for ( size_t i = 0; i < node.P_.n_receptors(); ++i )
  {
    const size_t j = i * S::NUM_STATE_ELEMENTS_PER_RECEPTOR;
    I_syn += y[ S::G + j ] * ( node.P_.E_rev[ i ] - V );
  }

  const double I_spike =
    node.P_.Delta_T == 0. ? 0 : ( node.P_.Delta_T * node.P_.g_L * std::exp( ( V - node.P_.V_th ) / node.P_.Delta_T ) );

  // dv/dt
  f[ S::V_M ] = is_refractory
    ? 0
    : ( -node.P_.g_L * ( V - node.P_.E_L ) + I_spike + I_syn - w + node.P_.I_e + node.B_.I_stim_ ) / node.P_.C_m;

  // Adaptation current w.
  f[ S::W ] = ( node.P_.a * ( V - node.P_.E_L ) - w ) / node.P_.tau_w;

  for ( size_t i = 0; i < node.P_.n_receptors(); ++i )
  {
    const size_t j = i * S::NUM_STATE_ELEMENTS_PER_RECEPTOR;
    // Synaptic conductance derivative dG/dt
    f[ S::DG + j ] = -y[ S::DG + j ] / node.P_.tau_syn[ i ];
    f[ S::G + j ] = y[ S::DG + j ] - y[ S::G + j ] / node.P_.tau_syn[ i ];
  }

  return GSL_SUCCESS;
}

/* ----------------------------------------------------------------
 * Default constructors defining default parameters and state
 * ---------------------------------------------------------------- */

aeif_cond_alpha_multisynapse::Parameters_::Parameters_()
  : V_peak_( 0.0 )    // mV
  , V_reset_( -60.0 ) // mV
  , t_ref_( 0.0 )     // ms
  , g_L( 30.0 )       // nS
  , C_m( 281.0 )      // pF
  , E_L( -70.6 )      // mV
  , Delta_T( 2.0 )    // mV
  , tau_w( 144.0 )    // ms
  , a( 4.0 )          // nS
  , b( 80.5 )         // pA
  , V_th( -50.4 )     // mV
  , tau_syn( 1, 2.0 ) // ms
  , E_rev( 1, 0.0 )   // mV
  , I_e( 0.0 )        // pA
  , gsl_error_tol( 1e-6 )
  , has_connections_( false )
{
}

aeif_cond_alpha_multisynapse::State_::State_( const Parameters_& p )
  : y_( STATE_VECTOR_MIN_SIZE, 0.0 )
  , r_( 0 )
{
  y_[ 0 ] = p.E_L;
}

/* ----------------------------------------------------------------
 * Parameter and state extractions and manipulation functions
 * ---------------------------------------------------------------- */

void
aeif_cond_alpha_multisynapse::Parameters_::get( DictionaryDatum& d ) const
{
  def< double >( d, names::C_m, C_m );
  def< double >( d, names::V_th, V_th );
  def< double >( d, names::t_ref, t_ref_ );
  def< double >( d, names::g_L, g_L );
  def< double >( d, names::E_L, E_L );
  def< double >( d, names::V_reset, V_reset_ );
  def< size_t >( d, names::n_receptors, n_receptors() );
  ArrayDatum E_rev_ad( E_rev );
  ArrayDatum tau_syn_ad( tau_syn );
  def< ArrayDatum >( d, names::E_rev, E_rev_ad );
  def< ArrayDatum >( d, names::tau_syn, tau_syn_ad );
  def< double >( d, names::a, a );
  def< double >( d, names::b, b );
  def< double >( d, names::Delta_T, Delta_T );
  def< double >( d, names::tau_w, tau_w );
  def< double >( d, names::I_e, I_e );
  def< double >( d, names::V_peak, V_peak_ );
  def< double >( d, names::gsl_error_tol, gsl_error_tol );
  def< bool >( d, names::has_connections, has_connections_ );
}

void
aeif_cond_alpha_multisynapse::Parameters_::set( const DictionaryDatum& d, Node* node )
{
  updateValueParam< double >( d, names::V_th, V_th, node );
  updateValueParam< double >( d, names::V_peak, V_peak_, node );
  updateValueParam< double >( d, names::t_ref, t_ref_, node );
  updateValueParam< double >( d, names::E_L, E_L, node );
  updateValueParam< double >( d, names::V_reset, V_reset_, node );

  updateValueParam< double >( d, names::C_m, C_m, node );
  updateValueParam< double >( d, names::g_L, g_L, node );

  const size_t old_n_receptors = n_receptors();
  bool Erev_flag = updateValue< std::vector< double > >( d, names::E_rev, E_rev );
  bool tau_flag = updateValue< std::vector< double > >( d, names::tau_syn, tau_syn );
  if ( Erev_flag or tau_flag )
  { // receptor arrays have been modified
    if ( ( E_rev.size() != old_n_receptors or tau_syn.size() != old_n_receptors )
      and ( not Erev_flag or not tau_flag ) )
    {
      throw BadProperty(
        "If the number of receptor ports is changed, both arrays "
        "E_rev and tau_syn must be provided." );
    }
    if ( E_rev.size() != tau_syn.size() )
    {
      throw BadProperty(
        "The reversal potential, and synaptic time constant arrays "
        "must have the same size." );
    }
    if ( tau_syn.size() < old_n_receptors and has_connections_ )
    {
      throw BadProperty(
        "The neuron has connections, therefore the number of ports cannot be "
        "reduced." );
    }
    for ( size_t i = 0; i < tau_syn.size(); ++i )
    {
      if ( tau_syn[ i ] <= 0 )
      {
        throw BadProperty( "All synaptic time constants must be strictly positive" );
      }
    }
  }

  updateValueParam< double >( d, names::a, a, node );
  updateValueParam< double >( d, names::b, b, node );
  updateValueParam< double >( d, names::Delta_T, Delta_T, node );
  updateValueParam< double >( d, names::tau_w, tau_w, node );

  updateValueParam< double >( d, names::I_e, I_e, node );

  updateValueParam< double >( d, names::gsl_error_tol, gsl_error_tol, node );

  if ( V_peak_ < V_th )
  {
    throw BadProperty( "V_peak >= V_th required." );
  }

  if ( V_reset_ >= V_peak_ )
  {
    throw BadProperty( "Ensure that: V_reset < V_peak ." );
  }

  if ( Delta_T < 0. )
  {
    throw BadProperty( "Delta_T must be positive." );
  }
  else if ( Delta_T > 0. )
  {
    // check for possible numerical overflow with the exponential divergence at
    // spike time, keep a 1e20 margin for the subsequent calculations
    const double max_exp_arg = std::log( std::numeric_limits< double >::max() / 1e20 );
    if ( ( V_peak_ - V_th ) / Delta_T >= max_exp_arg )
    {
      throw BadProperty(
        "The current combination of V_peak, V_th and Delta_T"
        "will lead to numerical overflow at spike time; try"
        "for instance to increase Delta_T or to reduce V_peak"
        "to avoid this problem." );
    }
  }

  if ( C_m <= 0 )
  {
    throw BadProperty( "Capacitance must be strictly positive." );
  }

  if ( t_ref_ < 0 )
  {
    throw BadProperty( "Refractory time cannot be negative." );
  }

  if ( tau_w <= 0 )
  {
    throw BadProperty( "All time constants must be strictly positive." );
  }

  if ( gsl_error_tol <= 0. )
  {
    throw BadProperty( "The gsl_error_tol must be strictly positive." );
  }
}

void
aeif_cond_alpha_multisynapse::State_::get( DictionaryDatum& d ) const
{
  def< double >( d, names::V_m, y_[ V_M ] );

  std::vector< double >* dg = new std::vector< double >();
  std::vector< double >* g = new std::vector< double >();

  for ( size_t i = 0;
        i < ( ( y_.size() - State_::NUMBER_OF_FIXED_STATES_ELEMENTS ) / State_::NUM_STATE_ELEMENTS_PER_RECEPTOR );
        ++i )
  {
    dg->push_back( y_[ State_::DG + ( State_::NUM_STATE_ELEMENTS_PER_RECEPTOR * i ) ] );
    g->push_back( y_[ State_::G + ( State_::NUM_STATE_ELEMENTS_PER_RECEPTOR * i ) ] );
  }

  ( *d )[ names::dg ] = DoubleVectorDatum( dg );
  ( *d )[ names::g ] = DoubleVectorDatum( g );

  def< double >( d, names::w, y_[ W ] );
}

void
aeif_cond_alpha_multisynapse::State_::set( const DictionaryDatum& d, Node* node )
{
  updateValueParam< double >( d, names::V_m, y_[ V_M ], node );
  updateValueParam< double >( d, names::w, y_[ W ], node );
}

aeif_cond_alpha_multisynapse::Buffers_::Buffers_( aeif_cond_alpha_multisynapse& n )
  : logger_( n )
  , s_( nullptr )
  , c_( nullptr )
  , e_( nullptr )
  , step_( Time::get_resolution().get_ms() )
  , IntegrationStep_( step_ )
  , I_stim_( 0.0 )
{
}

aeif_cond_alpha_multisynapse::Buffers_::Buffers_( const Buffers_& b, aeif_cond_alpha_multisynapse& n )
  : logger_( n )
  , s_( nullptr )
  , c_( nullptr )
  , e_( nullptr )
  , step_( b.step_ )
  , IntegrationStep_( b.IntegrationStep_ )
  , I_stim_( b.I_stim_ )
{
}

/* ----------------------------------------------------------------
 * Default and copy constructor for node, and destructor
 * ---------------------------------------------------------------- */

aeif_cond_alpha_multisynapse::aeif_cond_alpha_multisynapse()
  : ArchivingNode()
  , P_()
  , S_( P_ )
  , B_( *this )
{
  recordablesMap_.create( *this );
}

aeif_cond_alpha_multisynapse::aeif_cond_alpha_multisynapse( const aeif_cond_alpha_multisynapse& n )
  : ArchivingNode( n )
  , P_( n.P_ )
  , S_( n.S_ )
  , B_( n.B_, *this )
{
  recordablesMap_.create( *this );
}

aeif_cond_alpha_multisynapse::~aeif_cond_alpha_multisynapse()
{
  // GSL structs may not have been allocated, so we need to protect destruction
  if ( B_.s_ )
  {
    gsl_odeiv_step_free( B_.s_ );
  }
  if ( B_.c_ )
  {
    gsl_odeiv_control_free( B_.c_ );
  }
  if ( B_.e_ )
  {
    gsl_odeiv_evolve_free( B_.e_ );
  }
}

/* ----------------------------------------------------------------
 * Node initialization functions
 * ---------------------------------------------------------------- */

void
aeif_cond_alpha_multisynapse::init_buffers_()
{
  B_.spikes_.clear();   // includes resize
  B_.currents_.clear(); // includes resize
  ArchivingNode::clear_history();

  B_.logger_.reset();

  B_.step_ = Time::get_resolution().get_ms();
  B_.IntegrationStep_ =
    B_.step_; // reasonable initial value for numerical integrator step size; this will anyway be overwritten by
              // gsl_odeiv_evolve_apply(), but it might confuse the integrator if it contains uninitialised data

  if ( not B_.c_ )
  {
    B_.c_ = gsl_odeiv_control_yp_new( P_.gsl_error_tol, P_.gsl_error_tol );
  }
  else
  {
    gsl_odeiv_control_init( B_.c_, P_.gsl_error_tol, P_.gsl_error_tol, 0.0, 1.0 );
  }

  // Stepping function and evolution function are allocated in pre_run_hook()

  B_.sys_.function = aeif_cond_alpha_multisynapse_dynamics;
  B_.sys_.jacobian = nullptr;
  B_.sys_.params = reinterpret_cast< void* >( this );
  // B_.sys_.dimension is assigned in pre_run_hook()
  B_.I_stim_ = 0.0;
}

void
aeif_cond_alpha_multisynapse::pre_run_hook()
{
  // ensures initialization in case mm connected after Simulate
  B_.logger_.init();

  V_.g0_.resize( P_.n_receptors() );
  // g0_ will be initialized in the loop below.

  for ( size_t i = 0; i < P_.n_receptors(); ++i )
  {
    V_.g0_[ i ] = 1.0 * numerics::e / P_.tau_syn[ i ];
  }

  // set the right threshold depending on Delta_T
  if ( P_.Delta_T > 0. )
  {
    V_.V_peak = P_.V_peak_;
  }
  else
  {
    V_.V_peak = P_.V_th; // same as IAF dynamics for spikes if Delta_T == 0.
  }

  V_.refractory_counts_ = Time( Time::ms( P_.t_ref_ ) ).get_steps();

  B_.spikes_.resize( P_.n_receptors() );
  S_.y_.resize(
    State_::NUMBER_OF_FIXED_STATES_ELEMENTS + ( State_::NUM_STATE_ELEMENTS_PER_RECEPTOR * P_.n_receptors() ), 0.0 );

  // reallocate instance of stepping function for ODE GSL solver
  if ( B_.s_ )
  {
    gsl_odeiv_step_free( B_.s_ );
  }
  B_.s_ = gsl_odeiv_step_alloc( gsl_odeiv_step_rkf45, S_.y_.size() );

  // reallocate instance of evolution function for ODE GSL solver
  if ( B_.e_ )
  {
    gsl_odeiv_evolve_free( B_.e_ );
  }
  B_.e_ = gsl_odeiv_evolve_alloc( S_.y_.size() );

  B_.sys_.dimension = S_.y_.size();
}

/* ----------------------------------------------------------------
 * Update and spike handling functions
 * ---------------------------------------------------------------- */
void
aeif_cond_alpha_multisynapse::update( Time const& origin, const long from, const long to )
{
  assert( State_::V_M == 0 );

  for ( long lag = from; lag < to; ++lag ) // proceed by stepsize B_.step_
  {
    double t = 0.0; // internal time of the integration period

    // numerical integration with adaptive step size control:
    // ------------------------------------------------------
    // gsl_odeiv_evolve_apply performs only a single numerical
    // integration step, starting from t and bounded by step;
    // the while-loop ensures integration over the whole simulation
    // step (0, step] if more than one integration step is needed due
    // to a small integration step size;
    // note that (t+IntegrationStep > step) leads to integration over
    // (t, step] and afterwards setting t to step, but it does not
    // enforce setting IntegrationStep to step-t; this is of advantage
    // for a consistent and efficient integration across subsequent
    // simulation intervals

    while ( t < B_.step_ )
    {
      const int status = gsl_odeiv_evolve_apply( B_.e_,
        B_.c_,
        B_.s_,
        &B_.sys_,             // system of ODE
        &t,                   // from t
        B_.step_,             // to t <= step
        &B_.IntegrationStep_, // integration step size
        &S_.y_[ 0 ] );        // neuronal state converted to double[]

      if ( status != GSL_SUCCESS )
      {
        throw GSLSolverFailure( get_name(), status );
      }

      // check for unreasonable values; we allow V_M to explode
      if ( S_.y_[ State_::V_M ] < -1e3 or S_.y_[ State_::W ] < -1e6 or S_.y_[ State_::W ] > 1e6 )
      {
        throw NumericalInstability( get_name() );
      }

      if ( S_.r_ > 0 ) // if neuron is still in refractory period
      {
        S_.y_[ State_::V_M ] = P_.V_reset_; // clamp it to V_reset
      }
      else if ( S_.y_[ State_::V_M ] >= V_.V_peak ) // V_m >= V_peak: spike
      {
        S_.y_[ State_::V_M ] = P_.V_reset_;
        S_.y_[ State_::W ] += P_.b; // spike-driven adaptation

        /* Initialize refractory step counter.
         * - We need to add 1 to compensate for count-down immediately after
         *   while loop.
         * - If neuron has no refractory time, set to 0 to avoid refractory
         *   artifact inside while loop.
         */
        S_.r_ = V_.refractory_counts_ > 0 ? V_.refractory_counts_ + 1 : 0;

        set_spiketime( Time::step( origin.get_steps() + lag + 1 ) );
        SpikeEvent se;
        kernel().event_delivery_manager.send( *this, se, lag );
      }
    }

    if ( S_.r_ > 0 )
    {
      --S_.r_;
    }

    for ( size_t i = 0; i < P_.n_receptors(); ++i )
    {
      S_.y_[ State_::DG + ( State_::NUM_STATE_ELEMENTS_PER_RECEPTOR * i ) ] +=
        B_.spikes_[ i ].get_value( lag ) * V_.g0_[ i ]; // add incoming spike
    }
    // set new input current
    B_.I_stim_ = B_.currents_.get_value( lag );

    // log state data
    B_.logger_.record_data( origin.get_steps() + lag );

  } // for-loop
}

size_t
aeif_cond_alpha_multisynapse::handles_test_event( SpikeEvent&, size_t receptor_type )
{
  if ( receptor_type <= 0 or receptor_type > P_.n_receptors() )
  {
    throw IncompatibleReceptorType( receptor_type, get_name(), "SpikeEvent" );
  }
  P_.has_connections_ = true;
  return receptor_type;
}

void
aeif_cond_alpha_multisynapse::handle( SpikeEvent& e )
{
  if ( e.get_weight() < 0 )
  {
    throw BadProperty(
      "Synaptic weights for conductance-based multisynapse models "
      "must be positive." );
  }
  assert( e.get_delay_steps() > 0 );
  assert( ( e.get_rport() > 0 ) and ( ( size_t ) e.get_rport() <= P_.n_receptors() ) );

  B_.spikes_[ e.get_rport() - 1 ].add_value(
    e.get_rel_delivery_steps( kernel().simulation_manager.get_slice_origin() ), e.get_weight() * e.get_multiplicity() );
}

void
aeif_cond_alpha_multisynapse::handle( CurrentEvent& e )
{
  assert( e.get_delay_steps() > 0 );

  const double I = e.get_current();
  const double w = e.get_weight();

  // add weighted current; HEP 2002-10-04
  B_.currents_.add_value( e.get_rel_delivery_steps( kernel().simulation_manager.get_slice_origin() ), w * I );
}

void
aeif_cond_alpha_multisynapse::handle( DataLoggingRequest& e )
{
  B_.logger_.handle( e );
}

void
aeif_cond_alpha_multisynapse::set_status( const DictionaryDatum& d )
{
  Parameters_ ptmp = P_; // temporary copy in case of errors
  ptmp.set( d, this );   // throws if BadProperty
  State_ stmp = S_;      // temporary copy in case of errors
  stmp.set( d, this );   // throws if BadProperty

  // We now know that (ptmp, stmp) are consistent. We do not
  // write them back to (P_, S_) before we are also sure that
  // the properties to be set in the parent class are internally
  // consistent.
  ArchivingNode::set_status( d );

  /*
   * Here is where we must update the recordablesMap_ if new receptors
   * are added!
   */
  if ( ptmp.E_rev.size() > P_.E_rev.size() ) // Number of receptors increased
  {
    for ( size_t receptor = P_.E_rev.size(); receptor < ptmp.E_rev.size(); ++receptor )
    {
      size_t elem = aeif_cond_alpha_multisynapse::State_::G
        + receptor * aeif_cond_alpha_multisynapse::State_::NUM_STATE_ELEMENTS_PER_RECEPTOR;
      recordablesMap_.insert( get_g_receptor_name( receptor ), get_data_access_functor( elem ) );
    }
  }
  else if ( ptmp.E_rev.size() < P_.E_rev.size() )
  { // Number of receptors decreased
    for ( size_t receptor = ptmp.E_rev.size(); receptor < P_.E_rev.size(); ++receptor )
    {
      recordablesMap_.erase( get_g_receptor_name( receptor ) );
    }
  }

  // if we get here, temporaries contain consistent set of properties
  P_ = ptmp;
  S_ = stmp;
}

} // namespace nest

#endif // HAVE_GSL
