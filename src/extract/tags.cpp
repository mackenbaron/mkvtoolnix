/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tags from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cassert>
#include <iostream>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxTags.h>

#include "common/ebml.h"
#include "common/kax_analyzer.h"
#include "common/mm_io.h"
#include "common/mm_io_x.h"
#include "common/xml/ebml_tags_converter.h"
#include "extract/mkvextract.h"

using namespace libmatroska;

void
extract_tags(const std::string &file_name,
             kax_analyzer_c::parse_mode_e parse_mode) {
  auto analyzer      = open_and_analyze(file_name, parse_mode);
  ebml_master_cptr m = analyzer->read_all(EBML_INFO(KaxTags));
  if (!m)
    return;

  KaxTags *tags = dynamic_cast<KaxTags *>(m.get());
  assert(tags);

  mtx::xml::ebml_tags_converter_c::write_xml(*tags, *g_mm_stdio);
}
