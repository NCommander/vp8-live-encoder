/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "./webmenc.h"

#include <string>

#include "libwebm/mkvmuxer.hpp"
#include "libwebm/mkvmuxerutil.hpp"
#include "libwebm/mkvwriter.hpp"

#include "webm_live_muxer.h"
using namespace webm_tools;

namespace {
const uint64_t kDebugTrackUid = 0xDEADBEEF;
const int kVideoTrackNumber = 1;
const std::string app_writer = "vpxenc-live";
}  // namespace


void write_webm_file_header(struct EbmlGlobal *glob,
                            const vpx_codec_enc_cfg_t *cfg,
                            const struct vpx_rational *fps,
                            stereo_format_t stereo_fmt,
                            unsigned int fourcc,
                            const struct VpxRational *par) {


  WebMLiveMuxer * writer = new WebMLiveMuxer();
  writer->Init();
  writer->SetMuxingApp(app_writer);
  writer->SetWritingApp(app_writer);

  const char *codec_id;
  switch (fourcc) {
  case VP8_FOURCC:
    codec_id = "V_VP8";
    break;
  case VP9_FOURCC:
    codec_id = "V_VP9";
    break;
  case VP10_FOURCC:
    codec_id = "V_VP10";
    break;
  default:
    codec_id = "V_VP10";
    break;
  }

  std::string codec_string = codec_id;
  writer->AddVideoTrack(static_cast<int>(cfg->g_w),
                        static_cast<int>(cfg->g_h),
                        codec_string);

  glob->writer = writer;
}

/* Returns if chunk is ready to be written */
void write_webm_block(struct EbmlGlobal *glob,
                      const vpx_codec_enc_cfg_t *cfg,
                      const vpx_codec_cx_pkt_t *pkt) {
  int64_t pts_ns = pkt->data.frame.pts * 1000000000ll *
                   cfg->g_timebase.num / cfg->g_timebase.den;
  if (pts_ns <= glob->last_pts_ns)
    pts_ns = glob->last_pts_ns + 1000000;
  glob->last_pts_ns = pts_ns;
  static int skip = 1;

  WebMLiveMuxer * writer = reinterpret_cast<WebMLiveMuxer*>(glob->writer);

  writer->WriteVideoFrame(static_cast<uint8_t*>(pkt->data.frame.buf),
        pkt->data.frame.sz, pts_ns,
        pkt->data.frame.flags & VPX_FRAME_IS_KEY);
}

bool is_chunk_ready(struct EbmlGlobal *glob, int* chunk_size) {
  WebMLiveMuxer * writer = reinterpret_cast<WebMLiveMuxer*>(glob->writer);
  return writer->ChunkReady(chunk_size);
}

int read_chunk(struct EbmlGlobal *glob, int chunk_size, unsigned char* buffer) {
  WebMLiveMuxer * writer = reinterpret_cast<WebMLiveMuxer*>(glob->writer);
  return writer->ReadChunk(chunk_size, buffer);
}

void write_webm_file_footer(struct EbmlGlobal *glob) {
    WebMLiveMuxer * writer = reinterpret_cast<WebMLiveMuxer*>(glob->writer);
    writer->Finalize();
}
