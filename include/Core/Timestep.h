#pragma once

namespace Pyxis {
class Timestep {
  public:
    Timestep(double time) : m_Time(time) {}

    operator double() const { return m_Time; }

    double GetSeconds() const { return m_Time; }
    double GetMilliseconds() const { return m_Time * 1000.0f; }

  private:
    double m_Time;
};
} // namespace Pyxis
