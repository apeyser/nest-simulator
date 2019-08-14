/*
 *  recording_backend_memory.cpp
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

// Includes from nestkernel:
#include "recording_device.h"
#include "vp_manager_impl.h"

#include "recording_backend_memory.h"

nest::RecordingBackendMemory::RecordingBackendMemory()
{
}

nest::RecordingBackendMemory::~RecordingBackendMemory() throw()
{
}

void
nest::RecordingBackendMemory::initialize()
{
  device_data_map tmp( kernel().vp_manager.get_num_threads() );
  device_data_.swap( tmp );
}

void
nest::RecordingBackendMemory::finalize()
{
}

void
nest::RecordingBackendMemory::enroll( const RecordingDevice& device )
{
  thread t = device.get_thread();
  index gid = device.get_gid();

  device_data_map::value_type::iterator device_data = device_data_[ t ].find( gid );
  if ( device_data == device_data_[ t ].end() )
  {
    device_data_[ t ].insert( std::make_pair( gid, DeviceData() ) );
  }
}

void
nest::RecordingBackendMemory::disenroll( const RecordingDevice& device )
{
  thread t = device.get_thread();
  index gid = device.get_gid();

  device_data_map::value_type::iterator device_data = device_data_[ t ].find( gid );
  if ( device_data != device_data_[ t ].end() )
  {
    device_data_[ t ].erase( device_data );
  }
}

void
nest::RecordingBackendMemory::set_value_names( const RecordingDevice& device,
  const std::vector< Name >& double_value_names,
  const std::vector< Name >& long_value_names )
{
  const thread t = device.get_thread();
  const thread gid = device.get_gid();

  device_data_map::value_type::iterator device_data = device_data_[ t ].find( gid );
  assert( device_data != device_data_[ t ].end() );
  device_data->second.set_value_names( double_value_names, long_value_names );
}

void
nest::RecordingBackendMemory::pre_run_hook()
{
  // nothing to do
}

void
nest::RecordingBackendMemory::cleanup()
{
  // nothing to do
}

void
nest::RecordingBackendMemory::write( const RecordingDevice& device,
  const Event& event,
  const std::vector< double >& double_values,
  const std::vector< long >& long_values )
{
  thread t = device.get_thread();
  index gid = device.get_gid();

  device_data_map::value_type::iterator device_data = device_data_[ t ].find( gid );
  if ( device_data != device_data_[ t ].end() )
  {
    device_data->second.push_back( event, double_values, long_values );
  }

  // JME: why is this not working?
  // device_data_[ t ][ gid ].push_back( event, double_values, long_values );
}

void
nest::RecordingBackendMemory::get_device_status( const RecordingDevice& device, DictionaryDatum& d ) const
{
  const thread t = device.get_thread();
  const index gid = device.get_gid();

  device_data_map::value_type::const_iterator device_data = device_data_[ t ].find( gid );
  if ( device_data != device_data_[ t ].end() )
  {
    device_data->second.get_status( d );
  }
}

void
nest::RecordingBackendMemory::set_device_status( const RecordingDevice& device, const DictionaryDatum& d )
{
  const int t = device.get_thread();
  const int gid = device.get_gid();

  device_data_map::value_type::iterator device_data = device_data_[ t ].find( gid );
  if ( device_data != device_data_[ t ].end() )
  {
    device_data->second.set_status( d );
  }
}

void
nest::RecordingBackendMemory::post_run_hook()
{
  // nothing to do
}

void
nest::RecordingBackendMemory::get_status( lockPTRDatum< Dictionary, &SLIInterpreter::Dictionarytype >& ) const
{
  // nothing to do
}

void
nest::RecordingBackendMemory::set_status( lockPTRDatum< Dictionary, &SLIInterpreter::Dictionarytype > const& )
{
  // nothing to do
}

void
nest::RecordingBackendMemory::prepare()
{
  // nothing to do
}

/* ******************* Device meta data class DeviceInfo ******************* */

nest::RecordingBackendMemory::DeviceData::DeviceData()
  : time_in_steps_( false )
{
}

void
nest::RecordingBackendMemory::DeviceData::set_value_names( const std::vector< Name >& double_value_names,
  const std::vector< Name >& long_value_names )
{
  double_value_names_ = double_value_names;
  double_values_.resize( double_value_names.size() );

  long_value_names_ = long_value_names;
  long_values_.resize( long_value_names.size() );
}

void
nest::RecordingBackendMemory::DeviceData::push_back( const Event& event,
  const std::vector< double >& double_values,
  const std::vector< long >& long_values )
{
  senders_.push_back( event.get_sender_gid() );

  if ( time_in_steps_ )
  {
    times_steps_.push_back( event.get_stamp().get_steps() );
    times_offset_.push_back( event.get_offset() );
  }
  else
  {
    times_ms_.push_back( event.get_stamp().get_ms() - event.get_offset() );
  }

  for ( size_t i = 0; i < double_values.size(); ++i )
  {
    double_values_[ i ].push_back( double_values[ i ] );
  }
  for ( size_t i = 0; i < long_values.size(); ++i )
  {
    long_values_[ i ].push_back( long_values[ i ] );
  }
}

void
nest::RecordingBackendMemory::DeviceData::get_status( DictionaryDatum& d ) const
{
  DictionaryDatum events;

  if ( not d->known( names::events ) )
  {
    events = DictionaryDatum( new Dictionary );
    ( *d )[ names::events ] = events;
  }
  else
  {
    events = getValue< DictionaryDatum >( d, names::events );
  }

  initialize_property_intvector( events, names::senders );
  append_property( events, names::senders, senders_ );

  if ( time_in_steps_ )
  {
    initialize_property_intvector( events, names::times );
    append_property( events, names::times, times_steps_ );

    initialize_property_doublevector( events, names::offsets );
    append_property( events, names::offsets, times_offset_ );
  }
  else
  {
    initialize_property_doublevector( events, names::times );
    append_property( events, names::times, times_ms_ );
  }

  for ( size_t i = 0; i < double_values_.size(); ++i )
  {
    initialize_property_doublevector( events, double_value_names_[ i ] );
    append_property( events, double_value_names_[ i ], double_values_[ i ] );
  }
  for ( size_t i = 0; i < long_values_.size(); ++i )
  {
    initialize_property_intvector( events, long_value_names_[ i ] );
    append_property( events, long_value_names_[ i ], long_values_[ i ] );
  }

  ( *d )[ names::time_in_steps ] = time_in_steps_;
  ( *d )[ names::n_events ] = senders_.size();
}

void
nest::RecordingBackendMemory::DeviceData::set_status( const DictionaryDatum& d )
{
  updateValue< bool >( d, names::time_in_steps, time_in_steps_ );

  size_t n_events = 0;
  if ( updateValue< long >( d, names::n_events, n_events ) )
  {
    if ( n_events != 0 )
    {
      throw BadProperty( "Property n_events can only be set to 0 (which clears all stored events)." );
    }

    clear();
  }
}

void
nest::RecordingBackendMemory::DeviceData::clear()
{
  senders_.clear();
  times_ms_.clear();
  times_steps_.clear();
  times_offset_.clear();

  for ( size_t i = 0; i < double_values_.size(); ++i )
  {
    double_values_[ i ].clear();
  }
  for ( size_t i = 0; i < long_values_.size(); ++i )
  {
    long_values_[ i ].clear();
  }
}