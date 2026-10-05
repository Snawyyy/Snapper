#ifndef SNAPPER_MEDIA_FFMPEG_HANDLES_H_
#define SNAPPER_MEDIA_FFMPEG_HANDLES_H_

#include <QString>

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace snapper {

// Owning handles for FFmpeg objects: each frees itself, so no early
// return can leak one.
struct InputCloser final {
  void operator()(AVFormatContext* context) const;
};
struct OutputCloser final {
  void operator()(AVFormatContext* context) const;
};
struct CodecCloser final {
  void operator()(AVCodecContext* context) const;
};
struct FrameCloser final {
  void operator()(AVFrame* frame) const;
};
struct PacketCloser final {
  void operator()(AVPacket* packet) const;
};
struct ResamplerCloser final {
  void operator()(SwrContext* context) const;
};
struct ScalerCloser final {
  void operator()(SwsContext* context) const;
};

using InputHandle = std::unique_ptr<AVFormatContext, InputCloser>;
using OutputHandle = std::unique_ptr<AVFormatContext, OutputCloser>;
using CodecHandle = std::unique_ptr<AVCodecContext, CodecCloser>;
using FrameHandle = std::unique_ptr<AVFrame, FrameCloser>;
using PacketHandle = std::unique_ptr<AVPacket, PacketCloser>;
using ResamplerHandle = std::unique_ptr<SwrContext, ResamplerCloser>;
using ScalerHandle = std::unique_ptr<SwsContext, ScalerCloser>;

// FFmpeg's return codes, without its C macros (they trip C++ warnings).
bool IsAgain(int code);
bool IsEnd(int code);
QString AvErrorText(int code);

}  // namespace snapper

#endif  // SNAPPER_MEDIA_FFMPEG_HANDLES_H_
