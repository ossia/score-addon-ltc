/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "XWax.hpp"

#include <cmath>
#include <timecoder.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <numeric>
namespace ao
{

// Timecode format name mapping
static constexpr const char* timecode_names[] = {
    "serato_2a",      // Serato_2a
    "serato_2b",      // Serato_2b
    "serato_cd",      // Serato_CD
    "traktor_a",      // Traktor_A
    "traktor_b",      // Traktor_B
    "traktor_mk2_a",  // Traktor_MK2_A
    "traktor_mk2_b",  // Traktor_MK2_B
    "traktor_mk2_cd", // Traktor_MK2_CD
    "mixvibes_v2",    // MixVibes_V2
    "mixvibes_7inch", // MixVibes_7inch
    "pioneer_a",      // Pioneer_A
    "pioneer_b",      // Pioneer_B
};

XWaxDVS::XWaxDVS()
{
  m_timecoder = new timecoder{};
}

XWaxDVS::~XWaxDVS()
{
  clear_timecoder();
  delete m_timecoder;
}

const char* XWaxDVS::get_timecode_name() const
{
  const int idx = static_cast<int>(inputs.vinyl_type.value);
  if(idx >= 0 && idx < static_cast<int>(std::size(timecode_names)))
    return timecode_names[idx];
  return timecode_names[0]; // Default to Serato 2a
}

double XWaxDVS::get_speed_multiplier() const
{
  // RPM_33 = 1.0, RPM_45 = 1.35
  return (inputs.speed.value == Speed::RPM_45) ? 1.35 : 1.0;
}

void XWaxDVS::clear_timecoder()
{
  if(m_initialized && m_timecoder)
  {
    if(m_timecoder->mon)
    {
      timecoder_monitor_clear(m_timecoder);
    }
    timecoder_clear(m_timecoder);
    m_initialized = false;
  }
  m_timecode_def = nullptr;
}

void XWaxDVS::init_timecoder()
{
  clear_timecoder();

  if(m_setup.rate <= 0)
    return;

  const char* timecode_name = get_timecode_name();
  const double speed = get_speed_multiplier();

  // Use legacy pitch filter if AlphaBeta is selected
  using filter_type = std::decay_t<decltype(inputs.pitch_filter.value)>;
  const bool use_legacy_pitch = (inputs.pitch_filter.value == static_cast<filter_type>(1));

  // Find the timecode definition (this builds the LUT if needed)
  // Pass nullptr for lut_dir_path - xwax will use default or skip caching
  m_timecode_def = timecoder_find_definition(timecode_name, nullptr);

  if(!m_timecode_def)
  {
    // Fallback to serato_2a if the requested format fails
    m_timecode_def = timecoder_find_definition("serato_2a", nullptr);
    if(!m_timecode_def)
      return; // Give up if even the default fails
  }

  // Initialize the timecoder
  timecoder_init(
      m_timecoder,
      m_timecode_def,
      speed,
      static_cast<unsigned int>(m_setup.rate),
      false, // Not phono level (line level)
      use_legacy_pitch);

  m_initialized = true;

  // Reset quality tracking
  std::fill(std::begin(m_quality_ring), std::end(m_quality_ring), 0);
  m_quality_ring_index = 0;
  m_quality_ring_filled = 0;
  m_quality_last_position = -1;
  m_quality_last_pitch = 0.0;
}

void XWaxDVS::prepare(halp::setup setup)
{
  m_setup = setup;

  // Pre-allocate work buffer for a reasonable buffer size
  // This will be resized if needed during processing
  m_work_buffer.reserve(4096);

  // Force reinitialization on next process call
  m_last_vinyl_type = -1;
  m_last_speed = -1;
  m_last_pitch_filter = -1;
}

void XWaxDVS::operator()(halp::tick_flicks tk)
{
  const int frames = tk.frames;

  // Check if we need to reinitialize the timecoder due to parameter changes
  const int current_vinyl_type = static_cast<int>(inputs.vinyl_type.value);
  const int current_speed = static_cast<int>(inputs.speed.value);
  const int current_pitch_filter = static_cast<int>(inputs.pitch_filter.value);

  if(current_vinyl_type != m_last_vinyl_type ||
     current_speed != m_last_speed ||
     current_pitch_filter != m_last_pitch_filter)
  {
    init_timecoder();
    m_last_vinyl_type = current_vinyl_type;
    m_last_speed = current_speed;
    m_last_pitch_filter = current_pitch_filter;
  }

  // Cannot process without a valid timecoder
  if(!m_initialized || !m_timecoder)
  {
    outputs.speed = 0.0;
    outputs.tempo = 0.0;
    outputs.position = 0.0;
    outputs.timecode = -1;
    outputs.quality = 0.0;
    outputs.valid = false;
    return;
  }

  // We need at least 2 channels (stereo) for timecode decoding
  const int channels = inputs.audio.channels;
  if(channels < 2 || frames <= 0)
  {
    outputs.speed = 0.0;
    outputs.tempo = 0.0;
    outputs.position = 0.0;
    outputs.timecode = -1;
    outputs.quality = 0.0;
    outputs.valid = false;
    return;
  }

  // Resize work buffer if needed (stereo interleaved)
  const size_t required_size = static_cast<size_t>(frames) * 2;
  if(m_work_buffer.size() < required_size)
  {
    m_work_buffer.resize(required_size);
  }

  // Get channel pointers
  auto left_channel = inputs.audio.channel(0, frames);
  auto right_channel = inputs.audio.channel(1, frames);

  // Convert double samples to signed 16-bit shorts (interleaved stereo)
  // xwax expects samples in the full range of signed short (-32768 to 32767)
  constexpr double SAMPLE_SCALE = 32767.0;

  for(int i = 0; i < frames; ++i)
  {
    // Get samples and scale to 16-bit range
    double left_sample = left_channel[i] * SAMPLE_SCALE;
    double right_sample = right_channel[i] * SAMPLE_SCALE;

    // Clamp to valid range
    left_sample = std::clamp(left_sample, -32768.0, 32767.0);
    right_sample = std::clamp(right_sample, -32768.0, 32767.0);

    // Interleave into work buffer
    m_work_buffer[i * 2] = static_cast<signed short>(left_sample);
    m_work_buffer[i * 2 + 1] = static_cast<signed short>(right_sample);
  }

  // Submit samples to xwax timecoder
  timecoder_submit(m_timecoder, m_work_buffer.data(), static_cast<size_t>(frames));

  // Get pitch (speed) from the timecoder
  const double pitch = timecoder_get_pitch(m_timecoder);
  outputs.speed = pitch;
  outputs.tempo = pitch * inputs.tempo;

  // Get position from the timecoder
  double when = 0.0;
  const signed int position_ms = timecoder_get_position(m_timecoder, &when);

  if(position_ms >= 0)
  {
    // Valid position - convert from milliseconds to seconds and apply lead-in offset
    const double position_sec = static_cast<double>(position_ms) / 1000.0;
    const double leadin_sec = static_cast<double>(inputs.leadin.value);
    outputs.position = this->convert_output(position_sec - leadin_sec);
    outputs.timecode = this->convert_output(position_sec);
    outputs.valid = true;
  }
  else
  {
    // Invalid position
    outputs.position = 0.0;
    outputs.timecode = -1;
    outputs.valid = false;
  }

  // Calculate signal quality based on position and pitch stability
  // Similar to Mixxx's approach
  int position_quality = 0;
  if(position_ms == -1)
  {
    position_quality = 0;
  }
  else if(m_quality_last_position == -1 ||
          std::abs(position_ms - m_quality_last_position) >= 5)
  {
    // Position changed by at least 5ms - normal operation
    position_quality = 100;
  }
  else
  {
    // Position changed by less than 5ms - might indicate issues
    position_quality = 50;
  }
  m_quality_last_position = position_ms;

  int pitch_quality = 0;
  if(m_quality_ring_filled > 0)
  {
    const double pitch_diff = pitch - m_quality_last_pitch;
    if(pitch_diff != 0.0)
    {
      const double pitch_stability = std::abs(pitch / pitch_diff);
      if(pitch_stability < 3.0)
        pitch_quality = 0;
      else if(pitch_stability > 6.0)
        pitch_quality = 100;
      else
        pitch_quality = 75;
    }
  }
  m_quality_last_pitch = pitch;

  // Update quality ring buffer
  m_quality_ring[m_quality_ring_index] = position_quality + pitch_quality;
  m_quality_ring_index = (m_quality_ring_index + 1) % QUALITY_RING_SIZE;
  if(m_quality_ring_filled < QUALITY_RING_SIZE)
    m_quality_ring_filled++;

  // Calculate average quality
  const int quality_sum = std::accumulate(
      m_quality_ring, m_quality_ring + m_quality_ring_filled, 0);
  const double quality = static_cast<double>(quality_sum) /
                         (2.0 * 100.0 * static_cast<double>(m_quality_ring_filled));
  outputs.quality = std::clamp(quality, 0.0, 1.0);
}

}
