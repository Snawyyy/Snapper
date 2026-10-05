#include "media/ffmpeg_handles.h"

#include <array>
#include <cassert>
#include <cerrno>

namespace snapper {
namespace {

// FFmpeg's error macros use C casts; they are expanded only here.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
constexpr int kAgain = AVERROR(EAGAIN);
constexpr int kEnd = AVERROR_EOF;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

}  // namespace

void InputCloser::operator()(AVFormatContext* context) const {
  assert(context != nullptr);
  assert(context->iformat != nullptr);
  avformat_close_input(&context);
}

void OutputCloser::operator()(AVFormatContext* context) const {
  assert(context != nullptr);
  assert(context->oformat != nullptr);
  const bool has_file = context->pb != nullptr &&
                        !(context->oformat->flags & AVFMT_NOFILE);
  if (has_file) {
    avio_closep(&context->pb);
  }
  avformat_free_context(context);
}

void CodecCloser::operator()(AVCodecContext* context) const {
  assert(context != nullptr);
  assert(context->av_class != nullptr);
  avcodec_free_context(&context);
}

void FrameCloser::operator()(AVFrame* frame) const {
  assert(frame != nullptr);
  assert(frame->nb_samples >= 0);
  av_frame_free(&frame);
}

void PacketCloser::operator()(AVPacket* packet) const {
  assert(packet != nullptr);
  assert(packet->size >= 0);
  av_packet_free(&packet);
}

void ResamplerCloser::operator()(SwrContext* context) const {
  assert(context != nullptr);
  assert(swr_is_initialized(context) >= 0);
  swr_free(&context);
}

void ScalerCloser::operator()(SwsContext* context) const {
  assert(context != nullptr);
  assert(context->av_class != nullptr);
  sws_freeContext(context);
}

bool IsAgain(int code) {
  assert(code <= 0);
  assert(kAgain < 0);
  return code == kAgain;
}

bool IsEnd(int code) {
  assert(code <= 0);
  assert(kEnd < 0);
  return code == kEnd;
}

QString AvErrorText(int code) {
  assert(code < 0);
  std::array<char, AV_ERROR_MAX_STRING_SIZE> text{};
  const bool is_known = av_strerror(code, text.data(), text.size()) == 0;
  assert(text.back() == '\0');
  return is_known ? QString::fromUtf8(text.data())
                  : QStringLiteral("FFmpeg error %1").arg(code);
}

}  // namespace snapper
