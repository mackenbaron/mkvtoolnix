/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   PCM output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Patches by Robert Millan <rmh@aybabtu.com>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "pr_generic.h"
#include "p_pcm.h"
#include "matroska.h"

using namespace libmatroska;

pcm_packetizer_c::pcm_packetizer_c(generic_reader_c *_reader,
                                   int _samples_per_sec,
                                   int _channels,
                                   int _bits_per_sample,
                                   track_info_c &_ti,
                                   bool _big_endian,
                                   bool _float)
  throw (error_c):
  generic_packetizer_c(_reader, _ti),
  packetno(0), bps(_channels * _bits_per_sample * _samples_per_sec / 8),
  samples_per_sec(_samples_per_sec), channels(_channels),
  bits_per_sample(_bits_per_sample), packet_size(0),
  bytes_output(0), big_endian(_big_endian), ieee_float(_float) {

  int i;

  for (i = 32; i > 2; i >>= 2)
    if ((samples_per_sec % i) == 0)
      break;
  if ((i == 2) && ((packet_size % 5) == 0))
    i = 5;
  packet_size = samples_per_sec / i;

  set_track_type(track_audio);
  set_track_default_duration((int64_t)(1000000000.0 * packet_size / samples_per_sec));

  /* It could happen that (channels * bits_per_sample < 8).  Because of this,
     we mustn't divide by 8 in the same line, or the result would be hosed. */
  packet_size *= channels * bits_per_sample;
  packet_size /= 8;
}

pcm_packetizer_c::~pcm_packetizer_c() {
}

void
pcm_packetizer_c::set_headers() {
  if (ieee_float)
    set_codec_id(MKV_A_PCM_FLOAT);
  else if (big_endian)
    set_codec_id(MKV_A_PCM_BE);
  else
    set_codec_id(MKV_A_PCM);
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);

  generic_packetizer_c::set_headers();
}

int
pcm_packetizer_c::process(packet_cptr packet) {
  buffer.add(packet->data->get(), packet->data->get_size());

  while (buffer.get_size() >= packet_size) {
    add_packet(new packet_t(new memory_c(buffer.get_buffer(), packet_size, false), (int64_t)bytes_output * 1000000000ll / bps, (int64_t)packet_size * 1000000000ll / bps));
    buffer.remove(packet_size);
    bytes_output += packet_size;
  }

  return FILE_STATUS_MOREDATA;
}

void
pcm_packetizer_c::flush() {
  uint32_t size;

  size = buffer.get_size();
  if (size > 0) {
    add_packet(new packet_t(new memory_c(buffer.get_buffer(), size, false),
                            (int64_t)bytes_output * 1000000000ll / bps,
                            (int64_t)size * 1000000000ll / bps));
    bytes_output += size;
    buffer.remove(size);
  }

  generic_packetizer_c::flush();
}

connection_result_e
pcm_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 string &error_message) {
  pcm_packetizer_c *psrc;

  psrc = dynamic_cast<pcm_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  connect_check_a_samplerate(samples_per_sec, psrc->samples_per_sec);
  connect_check_a_channels(channels, psrc->channels);
  connect_check_a_bitdepth(bits_per_sample, psrc->bits_per_sample);
  return CAN_CONNECT_YES;
}
