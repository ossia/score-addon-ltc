#pragma once

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <vector>

// Forward declare the xwax timecoder struct
extern "C" {
struct timecoder;
struct timecode_def;
}

namespace ao
{
/**
 * @brief XWax DVS - Digital Vinyl System timecode decoder
 *
 * This processor takes stereo audio input from a DVS-compatible turntable
 * or CDJ and decodes the timecode signal to provide:
 * - Speed/pitch: The current playback speed relative to reference (1.0 = normal)
 * - Position: The absolute position on the timecode media in seconds
 * - Signal quality indicator
 *
 * Supports various timecode formats: Serato, Traktor, MixVibes, Pioneer
 */
struct XWaxDVS
{
  halp_meta(name, "XWax DVS")
  halp_meta(author, "ossia team")
  halp_meta(category, "Audio/Timing")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/xwax-dvs.html")
  halp_meta(c_name, "avnd_xwax_dvs")
  halp_meta(uuid, "e826a777-9c77-4fa5-a0e9-24ccf177a25c")

  enum VinylType
  {
    Serato_2a,
    Serato_2b,
    Serato_CD,
    Traktor_A,
    Traktor_B,
    Traktor_MK2_A,
    Traktor_MK2_B,
    Traktor_MK2_CD,
    MixVibes_V2,
    MixVibes_7inch,
    Pioneer_A,
    Pioneer_B
  };
  enum Speed
  {
    RPM_33,
    RPM_45
  };
  enum PitchFilter
  {
    Kalman,
    AlphaBeta
  };

  struct
  {
    halp::dynamic_audio_bus<"Timecode", double> audio;
    halp::combobox_t<"Vinyl Type", VinylType> vinyl_type;
    halp::enum_t<Speed, "Speed"> speed;
    halp::enum_t<PitchFilter, "Pitch Filter"> pitch_filter;
    halp::knob_f32<"Lead-in", halp::range{0.f, 60.f, 0.f}> leadin;
    halp::spinbox_f32<"Tempo", halp::range{0.f, 300.f, 120.f}> tempo;
  } inputs;

  struct
  {
    // Relative speed/pitch (-N to +N, 1.0 = normal forward, -1.0 = normal reverse)
    halp::val_port<"Speed", double> speed;

    // Speed * input tempo
    halp::val_port<"Tempo", double> tempo;

    // Absolute position in seconds (from start of timecode, accounting for lead-in)
    halp::val_port<"Position", double> position;

    // Raw timecode position in milliseconds (as reported by xwax)
    halp::val_port<"Timecode", int> timecode;

    // Signal quality (0.0 = no signal, 1.0 = excellent)
    halp::val_port<"Quality", double> quality;

    // Whether a valid position is currently available
    halp::val_port<"Valid", bool> valid;
  } outputs;

  XWaxDVS();
  ~XWaxDVS();

  void prepare(halp::setup setup);

  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk);

private:
  // Initialize or reinitialize the timecoder with current settings
  void init_timecoder();
  void clear_timecoder();

  // Get the xwax timecode name for the current vinyl type
  const char* get_timecode_name() const;

  // Get the speed multiplier for the current RPM setting
  double get_speed_multiplier() const;

  halp::setup m_setup{};

  // The xwax timecoder state (allocated dynamically to avoid including xwax headers)
  struct timecoder* m_timecoder{nullptr};
  struct timecode_def* m_timecode_def{nullptr};

  // Work buffer for converting double samples to 16-bit signed shorts
  std::vector<signed short> m_work_buffer;

  // Track the last settings to detect changes
  int m_last_vinyl_type{-1};
  int m_last_speed{-1};
  int m_last_pitch_filter{-1};

  // Quality tracking
  static constexpr int QUALITY_RING_SIZE = 32;
  int m_quality_ring[QUALITY_RING_SIZE]{};
  int m_quality_ring_index{0};
  int m_quality_ring_filled{0};
  int m_quality_last_position{-1};
  double m_quality_last_pitch{0.0};

  // Whether the timecoder has been initialized
  bool m_initialized{false};
};

}
