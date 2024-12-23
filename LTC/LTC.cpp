#include "LTC.hpp"
namespace ao
{

void LTCGenerator::prepare(halp::setup setup)
{
  this->setup = setup;
  update();
}

void LTCGenerator::update()
{
  double fps = 30.;
  switch(inputs.rate)
  {
    case LTC_TV_STANDARD::LTC_TV_525_60:
    case LTC_TV_STANDARD::LTC_TV_1125_60:
      fps = 30.;
      break;
    case LTC_TV_STANDARD::LTC_TV_625_50:
      fps = 25.;
      break;
    case LTC_TV_STANDARD::LTC_TV_FILM_24:
      fps = 24.;
      break;
  }

  if(!m_encoder)
    m_encoder = ltc_encoder_create(setup.rate, fps, inputs.rate, 0);
  else
    ltc_encoder_reinit(m_encoder, setup.rate, fps, inputs.rate, 0);

  {
    auto cur_seconds = m_current_flicks / 705'600'000.;
    auto smpte_offset = this->inputs.offset;
    auto offset = int64_t(cur_seconds + smpte_offset);
    const int days = std::div(offset, 86400L).quot;
    offset -= days * 86400;
    const int hours = std::div(offset, 3600L).quot;
    offset -= hours * 3600;
    const int minutes = std::div(offset, 60L).quot;
    offset -= minutes * 60;
    const int seconds = offset;

    SMPTETimecode st{
        .timezone = "+0000",
        .years = 0,
        .months = 0,
        .days = 0,
        .hours = (uint8_t)hours,
        .mins = (uint8_t)minutes,
        .secs = (uint8_t)seconds,
        .frame = 0};
    ltc_encoder_set_timecode(m_encoder, &st);
  }

  m_buffer = boost::circular_buffer<uint8_t>(ltc_encoder_get_buffersize(m_encoder) * 16);
}

LTCGenerator::~LTCGenerator()
{
  ltc_encoder_free(m_encoder);
}

void LTCGenerator::operator()(halp::tick_flicks tk)
{
  m_current_flicks = tk.start_in_flicks;
  // FIXME handle transport
  // FIXME handle live framerate chagne
  for(int frame = 0; frame < tk.frames; ++frame)
  {
    while(m_buffer.empty())
    {
      ltc_encoder_encode_byte(m_encoder, m_current_byte, 120. / tk.tempo);
      if(++m_current_byte == 10)
      {
        ltc_encoder_inc_timecode(m_encoder);
        m_current_byte = 0;
      }

      ltcsnd_sample_t* buf{};
      int len = ltc_encoder_get_bufferptr(m_encoder, &buf, true);
      for(int i = 0; i < len; i++)
        m_buffer.push_back(buf[i]);
    }

    ltcsnd_sample_t data = m_buffer.front();
    m_buffer.pop_front();
    outputs.audio[frame] = (data / 127.f) - 1.f;
  }
}
}
