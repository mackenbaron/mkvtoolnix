/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * class definition for the generic reader and packetizer
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include "os.h"

#include <deque>
#include <vector>

#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTags.h>

#include "compression.h"
#include "error.h"
#include "mm_io.h"

using namespace libmatroska;
using namespace std;

// CUES_* control the creation of cue entries for a track.
// UNSPECIFIED: is used for command line parsing.
// NONE:        don't create any cue entries.
// IFRAMES:     Create cue entries for all I frames.
// ALL:         Create cue entries for all frames (not really useful).
// SPARSE:      Create cue entries for I frames if no video track exists, but
//              create at most one cue entries every two seconds. Used for
//              audio only files.
#define CUES_UNSPECIFIED -1
#define CUES_NONE         0
#define CUES_IFRAMES      1
#define CUES_ALL          2
#define CUES_SPARSE       3

#define DEFTRACK_TYPE_AUDIO 0
#define DEFTRACK_TYPE_VIDEO 1
#define DEFTRACK_TYPE_SUBS  2

class generic_packetizer_c;
class generic_reader_c;

class memory_c {
public:
  unsigned char *data;
  uint32_t size;
  bool is_free;

public:
  memory_c(unsigned char *ndata, uint32_t nsize, bool nis_free):
    data(ndata), size(nsize), is_free(nis_free) {
    if (data == NULL)
      die("memory_c::memory_c: data = %p, size = %u\n", data, size);
  }
  memory_c(const memory_c &src) {
    die("memory_c::memory_c(const memory_c &) called\n");
  }
  ~memory_c() {
    release();
  }
  int lock() {
    is_free = false;
    return 0;
  }
  memory_c *grab() {
    if (size == 0)
      die("memory_c::grab(): size == 0\n");
    if (is_free) {
      is_free = false;
      return new memory_c(data, size, true);
    }
    return new memory_c((unsigned char *)safememdup(data, size), size, true);
  }
  int release() {
    if (is_free) {
      safefree(data);
      data = NULL;
      is_free = false;
    }
    return 0;
  }
};

typedef struct {
  int64_t displacement;
  double linear;
  int64_t id;
} audio_sync_t;

#define DEFAULT_TRACK_PRIOIRTY_NONE          0
#define DEFAULT_TRACK_PRIORITY_FROM_TYPE    10
#define DEFAULT_TRACK_PRIORITY_FROM_SOURCE  50
#define DEFAULT_TRACK_PRIORITY_CMDLINE     255  

#define FMT_FN "'%s': "
#define FMT_TID "'%s' track %lld: "

typedef struct {
  KaxBlockGroup *group;
  KaxBlock *block;
  KaxCluster *cluster;
  unsigned char *data;
  int length, superseeded, ref_priority;
  int64_t timecode, bref, fref, duration, packet_num, assigned_timecode;
  int64_t unmodified_assigned_timecode, unmodified_duration;
  bool duration_mandatory;
  generic_packetizer_c *source;
} packet_t;

typedef struct {
  int cues;
  int64_t id;
} cue_creation_t;

typedef struct {
  char *language;
  int64_t id;
} language_t;

typedef struct {
  char *file_name;
  int64_t id;
} tags_t;

typedef struct {
  float aspect_ratio;
  bool ar_factor;
  int width, height;
  int64_t id;
} display_properties_t;

typedef struct {
  char fourcc[5];
  int64_t id;
} fourcc_t;

typedef struct {
  int left, top, right, bottom;
  int64_t id;
} pixel_crop_t;

class track_info_c {
protected:
  bool initialized;
public:
  // The track ID.
  int64_t id;

  // Options used by the readers.
  char *fname;
  bool no_audio, no_video, no_subs;
  vector<int64_t> *atracks, *vtracks, *stracks;

  // Options used by the packetizers.
  unsigned char *private_data;
  int private_size;

  vector<fourcc_t> *all_fourccs;
  char fourcc[5];
  vector<display_properties_t> *display_properties;
  float aspect_ratio;
  int display_width, display_height;
  bool aspect_ratio_given, aspect_ratio_is_factor, display_dimensions_given;

  vector<audio_sync_t> *audio_syncs; // As given on the command line
  audio_sync_t async;           // For this very track

  vector<cue_creation_t> *cue_creations; // As given on the command line
  int cues;                     // For this very track

  vector<int64_t> *default_track_flags; // As given on the command line
  bool default_track;           // For this very track

  vector<language_t> *languages; // As given on the command line
  char *language;               // For this very track

  vector<language_t> *sub_charsets; // As given on the command line
  char *sub_charset;            // For this very track

  vector<tags_t> *all_tags;     // As given on the command line
  tags_t *tags_ptr;             // For this very track
  KaxTags *tags;                // For this very track

  vector<int64_t> *aac_is_sbr;  // For AAC+/HE-AAC/SBR

  vector<audio_sync_t> *packet_delays; // As given on the command line
  int64_t packet_delay;         // For this very track

  vector<cue_creation_t> *compression_list; // As given on the command line
  int compression;              // For this very track

  vector<language_t> *track_names; // As given on the command line
  char *track_name;             // For this very track

  vector<language_t> *all_ext_timecodes; // As given on the command line
  char *ext_timecodes;          // For this very track

  vector<pixel_crop_t> *pixel_crop_list; // As given on the command line
  pixel_crop_t pixel_cropping;  // For this very track

  bool no_chapters, no_attachments, no_tags;

  // The following variables are needed for the broken way of
  // syncing audio in AVIs: by prepending it with trash. Thanks to
  // the nandub author for this really, really sucky implementation.
  uint16_t avi_block_align;
  uint32_t avi_samples_per_sec;
  uint32_t avi_avg_bytes_per_sec;
  uint32_t avi_samples_per_chunk;
  vector<int64_t> *avi_block_sizes;

public:
  track_info_c();
  track_info_c(const track_info_c &src):
    initialized(false) {
    *this = src;
  }
  virtual ~track_info_c() {
    free_contents();
  }

  track_info_c &operator =(const track_info_c &src);
  virtual void free_contents();
};

class timecode_range_c {
public:
  int64_t start_frame, end_frame;
  double fps, base_timecode;

  bool operator <(const timecode_range_c &cmp) const {
    return start_frame < cmp.start_frame;
  }
};

typedef struct packetizer_container_t {
  generic_packetizer_c *orig;
  generic_packetizer_c *current;
} packetizer_container_t;

#define OPTZR(i) reader_packetizers[i].orig
#define PTZR(i) reader_packetizers[i].current
#define PTZR0 PTZR(0)
#define NPTZR() reader_packetizers.size()

class generic_reader_c {
public:
  track_info_c *ti;
  vector<packetizer_container_t> reader_packetizers;
  generic_reader_c *connected_to;
  int64_t max_timecode_seen;

public:
  generic_reader_c(track_info_c *nti);
  virtual ~generic_reader_c();

  virtual int read(generic_packetizer_c *ptzr, bool force = false) = 0;
  virtual int read_all();
  virtual int display_priority();
  virtual void display_progress(bool final = false);
  virtual void set_headers();
  virtual void set_headers_for_track(int64_t tid);
  virtual void identify() = 0;
  virtual void create_packetizer(int64_t tid) = 0;
  virtual void create_packetizers() {
    create_packetizer(0);
  }

  virtual void add_attachments(KaxAttachments *a) {
  }
  virtual int add_packetizer(generic_packetizer_c *ptzr);
  virtual void set_timecode_offset(int64_t offset);

protected:
  virtual bool demuxing_requested(char type, int64_t id);
};

#define CAN_CONNECT_YES            0
#define CAN_CONNECT_NO_FORMAT     -1
#define CAN_CONNECT_NO_PARAMETERS -2

class generic_packetizer_c {
protected:
  deque<packet_t *> packet_queue;

  int64_t initial_displacement;
  int64_t free_refs, enqueued_bytes;
  int64_t safety_last_timecode, safety_last_duration;

  KaxTrackEntry *track_entry;

  // Header entries. Can be set via set_XXX and will be 'rendered'
  // by set_headers().
  int hserialno, htrack_type, htrack_min_cache, htrack_max_cache;
  int64_t htrack_default_duration;
  bool default_track_warning_printed;
  uint32_t huid;

  char *hcodec_id;
  unsigned char *hcodec_private;
  int hcodec_private_length;

  float haudio_sampling_freq, haudio_output_sampling_freq;
  int haudio_channels, haudio_bit_depth;

  int hvideo_pixel_width, hvideo_pixel_height;
  int hvideo_display_width, hvideo_display_height;

  int64_t dumped_packet_number;

  int hcompression;
  compression_c *compressor;

  vector<timecode_range_c> *timecode_ranges;
  vector<int64_t> *ext_timecodes;
  uint32_t current_tc_range;
  int64_t frameno;
  int ext_timecodes_version;
  bool ext_timecodes_warning_printed;

  int64_t last_cue_timecode;

public:
  track_info_c *ti;
  generic_reader_c *reader;
  int64_t timecode_offset;

public:
  generic_packetizer_c(generic_reader_c *nreader, track_info_c *nti)
    throw (error_c);
  virtual ~generic_packetizer_c();

  virtual int read() {
    return reader->read(this);
  }
  virtual void reset() {
    track_entry = NULL;
  }

  virtual void add_packet(memory_c &mem, int64_t timecode,
                          int64_t duration, bool duration_mandatory = false,
                          int64_t bref = -1, int64_t fref = -1,
                          int ref_priority = -1);
  virtual packet_t *get_packet();
  virtual int packet_available() {
    return packet_queue.size();
  }
  virtual void flush() {
  }
  virtual int64_t get_smallest_timecode() {
    return (packet_queue.size() == 0) ? 0x0FFFFFFF :
      packet_queue.front()->timecode;
  }
  virtual int64_t get_queued_bytes() {
    return enqueued_bytes;
  }

  virtual void set_free_refs(int64_t nfree_refs) {
    free_refs = nfree_refs;
  }
  virtual int64_t get_free_refs() {
    return free_refs;
  }
  virtual void set_headers();
  virtual void fix_headers();
  virtual int process(memory_c &mem,
                      int64_t timecode = -1, int64_t length = -1,
                      int64_t bref = -1, int64_t fref = -1) = 0;

  virtual void dump_debug_info() = 0;

  virtual void set_cue_creation(int create_cue_data) {
    ti->cues = create_cue_data;
  }
  virtual int get_cue_creation() {
    return ti->cues;
  }
  virtual int64_t get_last_cue_timecode() {
    return last_cue_timecode;
  }
  virtual void set_last_cue_timecode(int64_t timecode) {
    last_cue_timecode = timecode;
  }

  virtual KaxTrackEntry *get_track_entry() {
    return track_entry;
  }
  virtual int get_track_num() {
    return hserialno;
  }
  virtual int64_t get_source_track_num() {
    return ti->id;
  }

  virtual int set_uid(uint32_t uid);
  virtual int get_uid() {
    return huid;
  }
  virtual void set_track_type(int type);
  virtual int get_track_type() {
    return htrack_type;
  }
  virtual void set_language(const char *language);

  virtual void set_codec_id(const char *id);
  virtual void set_codec_private(const unsigned char *cp, int length);

  virtual void set_track_min_cache(int min_cache);
  virtual void set_track_max_cache(int max_cache);
  virtual void set_track_default_duration(int64_t default_duration);
  virtual int64_t get_track_default_duration();

  virtual void set_audio_sampling_freq(float freq);
  virtual float get_audio_sampling_freq() {
    return haudio_sampling_freq;
  }
  virtual void set_audio_output_sampling_freq(float freq);
  virtual void set_audio_channels(int channels);
  virtual void set_audio_bit_depth(int bit_depth);

  virtual void set_video_pixel_width(int width);
  virtual void set_video_pixel_height(int height);
  virtual void set_video_display_width(int width);
  virtual void set_video_display_height(int height);
  virtual void set_video_aspect_ratio(float ar) {
    ti->aspect_ratio = ar;
  }
  virtual void set_video_pixel_cropping(int left, int top, int right,
                                        int bottom);

  virtual void set_as_default_track(int type, int priority);

  virtual void set_tag_track_uid();

  virtual void set_track_name(const char *name);

  virtual void set_default_compression_method(int method) {
    hcompression = method;
  }

  virtual int64_t get_next_timecode(int64_t timecode);
  virtual void parse_ext_timecode_file(const char *name);
  virtual void parse_ext_timecode_file_v1(mm_io_c *in, const char *name);
  virtual void parse_ext_timecode_file_v2(mm_io_c *in, const char *name);

  inline bool needs_negative_displacement(float) {
    return ((initial_displacement < 0) &&
            (ti->async.displacement > initial_displacement));
  }
  inline bool needs_positive_displacement(float duration) {
    return ((initial_displacement > 0) &&
            (iabs(ti->async.displacement - initial_displacement) >
             (duration / 2)));
  }
  virtual void displace(float by_ns);

  virtual void force_duration_on_last_packet();

  virtual const char *get_format_name() = 0;
  virtual int can_connect_to(generic_packetizer_c *src) = 0;

  virtual void enable_avi_audio_sync(bool enable) {
    if (enable && (ti->avi_block_sizes == NULL))
      ti->avi_block_sizes = new vector<int64_t>;
    else if (!enable && (ti->avi_block_sizes != NULL)) {
      delete ti->avi_block_sizes;
      ti->avi_block_sizes = NULL;
    }
  }
  virtual int64_t handle_avi_audio_sync(int64_t num_bytes, bool vbr);
  virtual void add_avi_block_size(int64_t block_size) {
    if (ti->avi_block_sizes != NULL)
      ti->avi_block_sizes->push_back(block_size);
  }

protected:
  virtual void dump_packet(const void *buffer, int size);
};

extern vector<generic_packetizer_c *> ptzrs_in_header_order;

#endif  // __PR_GENERIC_H
