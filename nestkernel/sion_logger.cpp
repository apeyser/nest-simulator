#include "recording_device.h"

#include "sion.h"

#include "sion_logger.h"

void
nest::SIONLogger::enroll( const int virtual_process, RecordingDevice& device )
{
  const Node& node = device.get_node();
  const int gid = node.get_gid();

  // is virtual_process == virtual process we are in?
  // FIXME: critical?

  if ( files_.find( virtual_process ) == files_.end() )
  {
    files_.insert( std::make_pair( virtual_process, VirtualProcessEntry() ) );
  }

  if ( files_[ virtual_process ].devices.find( gid ) == files_[ virtual_process ].devices.end() )
  {
    DeviceEntry entry( device );
    files_[ virtual_process ].devices.insert( std::make_pair( gid, entry ) );
  }
}

void
nest::SIONLogger::initialize()
{
  MPI_Comm local_comm;
  int rank;
  MPI_Comm_rank( MPI_COMM_WORLD, &rank );

#pragma omp parallel
  {
    Network& network = *( Node::network() );
    thread t = network.get_thread_id();
    int vp = network.thread_to_vp( t );

    std::string tmp = build_filename_();
    char* filename = strdup( tmp.c_str() );

    // SIONlib parameters
    int n_files = 1;
    sion_int32 fs_block_size = -1;

    sion_int64 sion_buffer_size = P_.sion_buffer_size_;

    int sid = sion_paropen_ompi( filename,
      "bw",
      &n_files,
      MPI_COMM_WORLD,
      &local_comm, // FIXME: does it do anything when not on JUQUEEN?
      &sion_buffer_size,
      &fs_block_size,
      &rank,
      NULL,
      NULL ); // FIXME: nullptr allowed here? Readback filename?

    VirtualProcessEntry& vpe = files_[ vp ];
    vpe.sid = sid;
    vpe.buffer.reserve( P_.buffer_size_ );
    vpe.buffer.clear();

    // TODO: write header
  }
}

void
nest::SIONLogger::finalize()
{
#pragma omp parallel
  {
    Network& network = *( Node::network() );
    thread t = network.get_thread_id();
    int vp = network.thread_to_vp( t );
    VirtualProcessEntry& entry = files_[ vp ];
    int& sid = entry.sid;
    SIONBuffer& buffer = entry.buffer;

    if ( buffer.get_size() > 0 )
    {
      sion_fwrite( buffer.read(), buffer.get_size(), 1, sid );
      buffer.clear();
    }

    sion_parclose_ompi( sid );

    // TODO: write header
  }
}

void
nest::SIONLogger::write( const RecordingDevice& device, const Event& event )
{
  const Node& node = device.get_node();
  int vp = node.get_vp();
  int gid = node.get_gid();

  // FIXME: use proper type for sender (was: index)
  const int sender = event.get_sender_gid();
  const Time stamp = event.get_stamp();
  const double offset = event.get_offset();

  VirtualProcessEntry& entry = files_[ vp ];
  int& sid = entry.sid;
  SIONBuffer& buffer = entry.buffer;

  double time = stamp.get_ms() - offset;
  int n_values = 0;

#pragma omp critical
  {
    std::cout << "writing event" << std::endl;
    std::cout << "  buffer size: " << buffer.get_capacity() << std::endl;
    std::cout << "  buffer free: " << buffer.get_free() << std::endl;
  }

  unsigned int required_space = 3 * sizeof( int ) + sizeof( double );
  if ( buffer.get_capacity() > required_space )
  {
    if ( buffer.get_free() < required_space )
    {
      sion_fwrite( buffer.read(), buffer.get_size(), 1, sid );
      buffer.clear();
    }

    buffer << gid << sender << time << n_values;
  }
  else
  {
    if ( buffer.get_size() > 0 )
    {
      sion_fwrite( buffer.read(), buffer.get_size(), 1, sid );
      buffer.clear();
    }

    sion_fwrite( &gid, sizeof( int ), 1, sid );
    sion_fwrite( &sender, sizeof( int ), 1, sid );
    sion_fwrite( &time, sizeof( double ), 1, sid );
    sion_fwrite( &n_values, sizeof( int ), 1, sid );
  }
}

void
nest::SIONLogger::write( const RecordingDevice& device,
  const Event& event,
  const std::vector< double_t >& values )
{
  const Node& node = device.get_node();
  int vp = node.get_vp();
  int gid = node.get_gid();

  const int sender = event.get_sender_gid();
  const Time stamp = event.get_stamp();
  const double offset = event.get_offset();

  VirtualProcessEntry& entry = files_[ vp ];
  int& sid = entry.sid;
  SIONBuffer& buffer = entry.buffer;

  double time = stamp.get_ms() - offset;
  int n_values = values.size();

#pragma omp critical
  {
    std::cout << "writing event" << std::endl;
    std::cout << "  buffer size: " << buffer.get_capacity() << std::endl;
    std::cout << "  buffer free: " << buffer.get_free() << std::endl;
  }

  unsigned int required_space = 3 * sizeof( int ) + ( 1 + n_values ) * sizeof( double );
  if ( buffer.get_capacity() > required_space )
  {
    if ( buffer.get_free() < required_space )
    {
      sion_fwrite( buffer.read(), buffer.get_size(), 1, sid );
      buffer.clear();
    }

    buffer << gid << sender << time << n_values;
    for ( std::vector< double_t >::const_iterator val = values.begin(); val != values.end(); ++val )
    {
      buffer << *val;
    }
  }
  else
  {
    if ( buffer.get_size() > 0 )
    {
      sion_fwrite( buffer.read(), buffer.get_size(), 1, sid );
      buffer.clear();
    }

    sion_fwrite( &gid, sizeof( int ), 1, sid );
    sion_fwrite( &sender, sizeof( int ), 1, sid );
    sion_fwrite( &time, sizeof( double ), 1, sid );
    sion_fwrite( &n_values, sizeof( int ), 1, sid );

    for ( std::vector< double_t >::const_iterator val = values.begin(); val != values.end(); ++val )
    {
      double value = *val;
      sion_fwrite( &value, sizeof( double ), 1, sid );
    }
  }
}

const std::string
nest::SIONLogger::build_filename_() const
{
  std::ostringstream basename;
  const std::string& path = Node::network()->get_data_path();
  if ( !path.empty() )
    basename << path << '/';
  basename << Node::network()->get_data_prefix();

  basename << "output";

  return basename.str() + '.' + P_.file_ext_;
}

/* ----------------------------------------------------------------
 * Parameter extraction and manipulation functions
 * ---------------------------------------------------------------- */

nest::SIONLogger::Parameters_::Parameters_()
  : file_ext_( "dat" )
  , sion_buffer_size_( 2400 )
  , buffer_size_( 1024 )
{
}

void
nest::SIONLogger::Parameters_::get( const SIONLogger& al, DictionaryDatum& d ) const
{
  ( *d )[ names::file_extension ] = file_ext_;
  ( *d )[ names::buffer_size ] = buffer_size_;
  ( *d )[ names::sion_buffer_size ] = sion_buffer_size_;
}

void
nest::SIONLogger::Parameters_::set( const SIONLogger& al, const DictionaryDatum& d )
{
  updateValue< std::string >( d, names::file_extension, file_ext_ );
  updateValue< long >( d, names::sion_buffer_size, sion_buffer_size_ );
  updateValue< long >( d, names::buffer_size, buffer_size_ );
}