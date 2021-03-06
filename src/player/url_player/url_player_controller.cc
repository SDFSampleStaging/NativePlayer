/*!
 * url_player_controller.cc (https://github.com/SamsungDForum/NativePlayer)
 * Copyright 2016, Samsung Electronics Co., Ltd
 * Licensed under the MIT license
 *
 * @author Tomasz Borkowski
 * @author Michal Murgrabia
 */

#include "player/url_player/url_player_controller.h"

#include <functional>
#include <limits>
#include <utility>

#include "nacl_player/error_codes.h"
#include "nacl_player/url_data_source.h"
#include "ppapi/cpp/var_dictionary.h"

using Samsung::NaClPlayer::ErrorCodes;
using Samsung::NaClPlayer::URLDataSource;
using Samsung::NaClPlayer::MediaDataSource;
using Samsung::NaClPlayer::MediaPlayer;
using Samsung::NaClPlayer::MediaPlayerState;
using Samsung::NaClPlayer::Rect;
using Samsung::NaClPlayer::TextTrackInfo;
using Samsung::NaClPlayer::TimeTicks;
using std::make_shared;
using std::placeholders::_1;

void UrlPlayerController::InitPlayer(const std::string& url,
                                     const std::string& subtitle,
                                     const std::string& encoding) {
  LOG("Loading media from : [%s]", url.c_str());
  CleanPlayer();

  player_thread_ = MakeUnique<pp::SimpleThread>(instance_);
  player_thread_->Start();

  player_ = make_shared<MediaPlayer>();

  // initialize a listeners and register them in player
  listeners_.player_listener =
      make_shared<MediaPlayerListener>(message_sender_);
  listeners_.buffering_listener =
      make_shared<MediaBufferingListener>(message_sender_);
  listeners_.subtitle_listener =
      make_shared<SubtitleListener>(message_sender_);

  player_->SetMediaEventsListener(listeners_.player_listener);
  player_->SetBufferingListener(listeners_.buffering_listener);
  player_->SetSubtitleListener(listeners_.subtitle_listener);

  // register subtitles source if defined
  if (!subtitle.empty()) {
    text_track_ = MakeUnique<TextTrackInfo>();
    int32_t ret = player_->AddExternalSubtitles(subtitle,
                                                encoding,
                                                *text_track_);
    if (ret != ErrorCodes::Success) {
      LOG_ERROR("Failed to initialize subtitles, code: "
          "%d path: %s, encoding: %s", ret, subtitle.c_str(), encoding.c_str());
    }
  }

  int32_t ret = player_->SetDisplayRect(view_rect_);

  if (ret != ErrorCodes::Success) {
    LOG_ERROR("Failed to set display rect [(%d - %d) (%d - %d)], code: %d",
       view_rect_.x(), view_rect_.y(), view_rect_.width(), view_rect_.height(),
       ret);
  }

  InitializeUrlPlayer(url);
}

void UrlPlayerController::InitializeUrlPlayer(
    const std::string& content_container_url) {
  LOG("Play content directly from URL = %s ", content_container_url.c_str());
  data_source_ = make_shared<URLDataSource>(content_container_url);
  player_->AttachDataSource(*data_source_);
  TimeTicks duration;
  if (player_->GetDuration(duration) == ErrorCodes::Success) {
    message_sender_->SetMediaDuration(duration);
    LOG("Got duration: %f [s].", duration);
  } else {
    LOG("Failed to retreive duration!");
  }
  PostTextTrackInfo();
}

void UrlPlayerController::Play() {
  if (!player_) {
    LOG("Play. player is not initialized, cannot play");
    return;
  }

  int32_t ret = player_->Play();
  if (ret == ErrorCodes::Success) {
    LOG("Play called successfully");
  } else {
    LOG_ERROR("Play call failed, code: %d", ret);
  }
}

void UrlPlayerController::Pause() {
  if (!player_) {
    LOG("Pause. player is not initialized");
    return;
  }

  int32_t ret = player_->Pause();
  if (ret == ErrorCodes::Success) {
    LOG("Pause called successfully");
  } else {
    LOG_ERROR("Pause call failed, code: %d", ret);
  }
}

void UrlPlayerController::Seek(TimeTicks to_time) {
  LOG("Seek to %f", to_time);
  int32_t ret =
      player_->Seek(to_time, std::bind(&UrlPlayerController::OnSeek, this, _1));
  if (ret < ErrorCodes::CompletionPending) {
    LOG_ERROR("Seek call failed, code: %d", ret);
  }
}

void UrlPlayerController::ChangeRepresentation(StreamType /*stream_type*/,
                                               int32_t /*id*/) {
  LOG("URLplayer doesnt support changing representation");
}

void UrlPlayerController::SetViewRect(const Rect& view_rect) {
  view_rect_ = view_rect;
  if (!player_) return;

  LOG_DEBUG("Set view rect to %d, %d", view_rect_.width(), view_rect_.height());
  int32_t ret = player_->SetDisplayRect(
      view_rect_, std::bind(&UrlPlayerController::OnSetDisplayRect, this, _1));
  if (ret < ErrorCodes::CompletionPending)
    LOG_ERROR("SetDisplayRect result: %d", ret);
}

void UrlPlayerController::PostTextTrackInfo() {
  int32_t ret = player_->GetTextTracksList(text_track_list_);
  if (ret == ErrorCodes::Success) {
    LOG("GetTextTrackInfo called successfully");
    message_sender_->SetTextTracks(text_track_list_);
  } else {
    LOG_ERROR("GetTextTrackInfo call failed, code: %d", ret);
  }
}

void UrlPlayerController::ChangeSubtitles(int32_t id) {
  LOG("Change subtitle to %d", id);
  player_thread_->message_loop().PostWork(
      cc_factory_.NewCallback(
          &UrlPlayerController::OnChangeSubtitles, id));
}

void UrlPlayerController::ChangeSubtitleVisibility() {
  subtitles_visible_ = !subtitles_visible_;
  LOG("Change subtitle visibility to %d", subtitles_visible_);
  player_thread_->message_loop().PostWork(
      cc_factory_.NewCallback(
          &UrlPlayerController::OnChangeSubVisibility,
          subtitles_visible_));
}

PlayerController::PlayerState UrlPlayerController::GetState() {
  return state_;
}

void UrlPlayerController::OnSetDisplayRect(int32_t ret) {
  LOG_DEBUG("SetDisplayRect result: %d", ret);
}

void UrlPlayerController::OnSeek(int32_t ret) {
  TimeTicks current_playback_time = 0.0;
  player_->GetCurrentTime(current_playback_time);
  LOG("After seek time: %f, result: %d", current_playback_time, ret);
}

void UrlPlayerController::OnChangeSubtitles(int32_t, int32_t id) {
  int32_t ret =
      player_->SelectTrack(Samsung::NaClPlayer::ElementaryStreamType_Text, id);
  if (ret == ErrorCodes::Success) {
    LOG("SelectTrack called successfully");
  } else {
    LOG_ERROR("SelectTrack call failed, code: %d", ret);
  }
}

void UrlPlayerController::OnChangeSubVisibility(int32_t, bool show) {
  if (show)
    player_->SetSubtitleListener(listeners_.subtitle_listener);
  else
    player_->SetSubtitleListener(nullptr);
}

void UrlPlayerController::CleanPlayer() {
  LOG("Cleaning player.");
  if (player_) return;
  data_source_.reset();
  state_ = PlayerState::kUnitialized;
}
