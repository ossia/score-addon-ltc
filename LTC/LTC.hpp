#pragma once

#include <boost/circular_buffer.hpp>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cmath>
#include <ltc.h>

namespace ao
{
struct LTCGenerator
{
  halp_meta(name, "LTC Generator")
  halp_meta(c_name, "avnd_ltc_gen")
  halp_meta(uuid, "f87bec01-d2c1-4bdf-bda7-792bc62b0c49")

  struct
  {
    struct : halp::spinbox_i32<"Offset (s)", halp::irange{-128000, 128000, 0}>
    {
      void update(LTCGenerator& self) { self.update(); }
    } offset;

    struct : halp::enum_t<LTC_TV_STANDARD, "Framerate">
    {
      struct range
      {
        std::string_view values[4]
            = {"525 (30fps)", "625 (25 fps)", "1125 (30fps)", "Film (24 fps)"};
        LTC_TV_STANDARD init{};
      };

      void update(LTCGenerator& self) { self.update(); }
    } rate;
  } inputs;

  struct
  {
    halp::audio_channel<"LTC", double> audio;
  } outputs;

  void prepare(halp::setup setup);
  void update();

  ~LTCGenerator();
  using tick = halp::tick_flicks;
  void operator()(halp::tick_flicks tk);

private:
  halp::setup setup;
  LTCEncoder* m_encoder{};

  int m_current_byte = 0;
  boost::circular_buffer<uint8_t> m_buffer;
  int64_t m_current_flicks{};
};
}
