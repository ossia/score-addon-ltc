#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ltc.h>

#include <chrono>
#include <cmath>

namespace ao
{

/**
 * LTC (Linear Timecode) Input
 * Decodes LTC timecode from audio input
 * Outputs timecode values, frame rate, and validity status
 */
struct LTCInput
{
  halp_meta(name, "LTC Input")
  halp_meta(author, "ossia team")
  halp_meta(category, "Audio/Timing")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/ltc-input.html")
  halp_meta(c_name, "avnd_ltc_input")
  halp_meta(uuid, "31423401-13d3-4732-92a8-13e9d7aa56db")
  halp_meta(description, "Decode LTC (Linear Timecode) from audio input")

  enum class FrameRate
  {
    FPS_24 = 0,
    FPS_25 = 1,
    FPS_2997 = 2,
    FPS_30 = 3,
    Auto = 4
  };

  enum class OutputFormat
  {
    Seconds,
    Milliseconds,
    Microseconds,
    Nanoseconds,
    Flicks
  };

  struct
  {
    halp::audio_channel<"LTC Audio", double> audio;

    struct : halp::spinbox_i32<"Offset (s)", halp::irange{-128000, 128000, 0}>
    {
    } offset;

    halp::combobox_t<"Output Format", OutputFormat> format{OutputFormat::Seconds};
    halp::combobox_t<"Framerate", FrameRate> framerate{FrameRate::Auto};

    struct : halp::spinbox_i32<"Queue Size", halp::irange{8, 256, 32}>
    {
      void update(LTCInput& self) { self.reinit_decoder(); }
    } queue_size;
  } inputs;

  struct
  {
    halp::val_port<"Timecode", double> timecode{0.0};
    halp::val_port<"Valid", bool> valid{false};
    halp::val_port<"Frame Rate", double> frame_rate{30.0};
    halp::val_port<"Drop Frame", bool> drop_frame{false};
    halp::val_port<"Reverse", bool> reverse{false};
    halp::val_port<"Volume (dBFS)", double> volume{-96.0};
  } outputs;

  halp::setup m_setup{};

  void prepare(halp::setup setup)
  {
    m_setup = setup;
    reinit_decoder();
  }

  void reinit_decoder()
  {
    if(m_decoder)
    {
      ltc_decoder_free(m_decoder);
      m_decoder = nullptr;
    }

    if(m_setup.rate <= 1)
      return;

    // Calculate audio frames per video frame (assuming 30fps as default)
    // This is just an initial estimate; libltc tracks speed dynamically
    int apv = static_cast<int>(m_setup.rate / 30.0);
    m_decoder = ltc_decoder_create(apv, inputs.queue_size.value);
    m_sample_position = 0;
    m_last_valid_time = std::chrono::steady_clock::now();
  }

  ~LTCInput()
  {
    if(m_decoder)
    {
      ltc_decoder_free(m_decoder);
      m_decoder = nullptr;
    }
  }

  // Get the actual frame rate from LTC frame flags
  static double get_frame_rate_from_standard(LTC_TV_STANDARD standard, bool drop_frame)
  {
    switch(standard)
    {
      case LTC_TV_525_60:
      case LTC_TV_1125_60:
        return drop_frame ? 29.97 : 30.0;
      case LTC_TV_625_50:
        return 25.0;
      case LTC_TV_FILM_24:
        return 24.0;
      default:
        return 30.0;
    }
  }

  // Detect TV standard from frame rate setting or auto-detect
  LTC_TV_STANDARD detect_standard(const LTCFrame& frame) const
  {
    if(inputs.framerate.value != FrameRate::Auto)
    {
      switch(inputs.framerate.value)
      {
        case FrameRate::FPS_24:
          return LTC_TV_FILM_24;
        case FrameRate::FPS_25:
          return LTC_TV_625_50;
        case FrameRate::FPS_2997:
        case FrameRate::FPS_30:
        default:
          return LTC_TV_525_60;
      }
    }

    // Auto-detect based on frame values
    // If frames go up to 24, it's 25fps
    // If drop-frame bit is set, it's 29.97
    // Otherwise assume 30fps (or 24fps for film)
    if(frame.dfbit)
      return LTC_TV_525_60; // 29.97 drop-frame

    // Heuristic: check max frame value seen
    int max_frame = frame.frame_tens * 10 + frame.frame_units;
    if(max_frame >= 25)
      return LTC_TV_525_60; // 30fps
    else if(max_frame >= 24)
      return LTC_TV_625_50; // 25fps
    else
      return LTC_TV_FILM_24; // 24fps (or could be higher, but we can't tell yet)
  }

  double get_configured_fps() const
  {
    switch(inputs.framerate.value)
    {
      case FrameRate::FPS_24:
        return 24.0;
      case FrameRate::FPS_25:
        return 25.0;
      case FrameRate::FPS_2997:
        return 29.97;
      case FrameRate::FPS_30:
        return 30.0;
      case FrameRate::Auto:
      default:
        return 30.0;
    }
  }

  double to_seconds(const SMPTETimecode& tc, double fps) const
  {
    double total_seconds
        = tc.hours * 3600.0 + tc.mins * 60.0 + tc.secs + (tc.frame / fps);
    return total_seconds + inputs.offset.value;
  }

  double convert_output(double seconds) const
  {
    switch(inputs.format.value)
    {
      case OutputFormat::Seconds:
        return seconds;
      case OutputFormat::Milliseconds:
        return seconds * 1000.0;
      case OutputFormat::Microseconds:
        return seconds * 1e6;
      case OutputFormat::Nanoseconds:
        return seconds * 1e9;
      case OutputFormat::Flicks:
        return seconds * 705'600'000.0;
      default:
        return seconds;
    }
  }

  void check_timeout()
  {
    auto now = std::chrono::steady_clock::now();
    auto elapsed
        = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_valid_time)
              .count();

    // If no valid frame for more than 500ms, mark as invalid
    if(elapsed > 500)
    {
      outputs.valid = false;
    }
  }

  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk)
  {
    if(!m_decoder)
      return;

    const int frames = tk.frames;
    if(frames <= 0)
      return;

    // Feed audio samples to the decoder
    // We need to convert double samples to what libltc expects
    // libltc provides ltc_decoder_write_double for this
    ltc_decoder_write_double(
        m_decoder, inputs.audio.channel, static_cast<size_t>(frames), m_sample_position);

    m_sample_position += frames;

    // Read any decoded frames from the queue
    LTCFrameExt ltc_frame;
    bool got_frame = false;

    // Process all available frames, keeping the most recent
    while(ltc_decoder_read(m_decoder, &ltc_frame) > 0)
    {
      got_frame = true;
      m_last_frame = ltc_frame;
    }

    if(got_frame)
    {
      m_last_valid_time = std::chrono::steady_clock::now();

      // Convert LTC frame to SMPTE timecode
      SMPTETimecode tc;
      ltc_frame_to_time(&tc, &m_last_frame.ltc, 0);

      // Detect frame rate
      LTC_TV_STANDARD standard = detect_standard(m_last_frame.ltc);
      bool is_drop_frame = m_last_frame.ltc.dfbit != 0;
      double fps = get_frame_rate_from_standard(standard, is_drop_frame);

      if(inputs.framerate.value != FrameRate::Auto)
      {
        fps = get_configured_fps();
      }

      // Update outputs
      outputs.frame_rate = fps;
      outputs.drop_frame = is_drop_frame;
      outputs.reverse = m_last_frame.reverse != 0;
      outputs.volume = m_last_frame.volume;

      double seconds = to_seconds(tc, fps);
      outputs.timecode = convert_output(seconds);

      outputs.valid = true;
    }

    // Check for timeout
    check_timeout();
  }

private:
  LTCDecoder* m_decoder{};
  ltc_off_t m_sample_position{0};
  LTCFrameExt m_last_frame{};
  std::chrono::steady_clock::time_point m_last_valid_time{};
};

} // namespace ao
