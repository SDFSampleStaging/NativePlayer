/*!
 * ffmpeg_demuxer.h (https://github.com/SamsungDForum/NativePlayer)
 * Copyright 2016, Samsung Electronics Co., Ltd
 * Licensed under the MIT license
 *
 * @author Jacob Tarasiewicz
 * @author Tomasz Borkowski
 */

#ifndef SRC_PLAYER_ES_DASH_PLAYER_DEMUXER_FFMPEG_DEMUXER_H_
#define SRC_PLAYER_ES_DASH_PLAYER_DEMUXER_FFMPEG_DEMUXER_H_

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"
#include "ppapi/utility/threading/lock.h"
#include "nacl_player/media_common.h"

extern "C" {
#include "libavformat/avformat.h"
}

#include "demuxer/stream_demuxer.h"

class FFMpegDemuxer : public StreamDemuxer {
 public:
  typedef std::function<void(StreamDemuxer::Message,
      std::unique_ptr<ElementaryStreamPacket>)> InitCallback;
  typedef std::function<void(const std::string&,
      const std::vector<uint8_t>& init_data)> DrmInitCallback;

  explicit FFMpegDemuxer(const pp::InstanceHandle& instance,
                         uint32_t probe_size, Type type);
  ~FFMpegDemuxer();

  bool Init(const InitCallback& callback,
            pp::MessageLoop callback_dispatcher) override;
  void Flush() override;
  void Parse(const std::vector<uint8_t>& data) override;
  bool SetAudioConfigListener(
      const std::function<void(const AudioConfig&)>& callback) override;
  bool SetVideoConfigListener(
      const std::function<void(const VideoConfig&)>& callback) override;
  bool SetDRMInitDataListener(const DrmInitCallback& callback) override;
  void SetTimestamp(Samsung::NaClPlayer::TimeTicks) override;
  void Close() override;
  int Read(uint8_t* data, int size);

 private:
  typedef std::tuple<
      StreamDemuxer::Message,
      std::unique_ptr<ElementaryStreamPacket>> EsPktCallbackData;
  static constexpr uint32_t kEsPktCallbackDataMessage = 0;
  static constexpr uint32_t kEsPktCallbackDataPacket = 1;

  void StartParsing(int32_t);
  void CallbackInDispatcherThread(int32_t, StreamDemuxer::Message msg);
  void DispatchCallback(StreamDemuxer::Message);
  void EsPktCallbackInDispatcherThread(int32_t,
      const std::shared_ptr<EsPktCallbackData>& data);
  void DrmInitCallbackInDispatcherThread(int32_t, const std::string& type,
      const std::vector<uint8_t>& init_data);
  bool InitStreamInfo();
  static void InitFFmpeg();
  bool ParseH264SpsPpsFromExtraData(const uint8_t* extra_data,
                                    const int extra_data_size);

  std::unique_ptr<ElementaryStreamPacket> MakeESPacketFromAVPacket(
      AVPacket* pkt);

  int PrepareAACHeader(const uint8_t* data, size_t length);
  void UpdateVideoConfig();
  void UpdateAudioConfig();
  void UpdateContentProtectionConfig();
  void CallbackConfigInDispatcherThread(int32_t, Type type);
  std::function<void(const VideoConfig&)> video_config_callback_;
  std::function<void(const AudioConfig&)> audio_config_callback_;
  std::function<void(const std::string& type,
                     const std::vector<uint8_t>& init_data)>
      drm_init_data_callback_;
  std::function<void(StreamDemuxer::Message,
                     std::unique_ptr<ElementaryStreamPacket>)>
      es_pkt_callback_;

  Type stream_type_;
  int audio_stream_idx_;
  int video_stream_idx_;
  pp::SimpleThread parser_thread_;
  pp::CompletionCallbackFactory<FFMpegDemuxer> callback_factory_;

  VideoConfig video_config_;
  AudioConfig audio_config_;
  AVFormatContext* format_context_;
  AVIOContext* io_context_;

  pp::Lock buffer_lock_;
  pp::MessageLoop callback_dispatcher_;
  std::vector<uint8_t> buffer_;
  // in case of performance issues, buffer_ may be changed to a list of
  // buffers to reduce amount of data copying
  bool context_opened_;
  bool streams_initialized_;
  bool end_of_file_;
  bool exited_;
  uint32_t probe_size_;
  Samsung::NaClPlayer::TimeTicks timestamp_;
};

#endif  // SRC_PLAYER_ES_DASH_PLAYER_DEMUXER_FFMPEG_DEMUXER_H_
