#include "GSTOutputPlugin.hxx"
#include "GSTOutput.hxx"
#include "../Wrapper.hxx"

typedef AudioOutputWrapper<GSTOutput> Wrapper;

const struct AudioOutputPlugin gst_output_plugin =
{
  "gstreamer",
  nullptr,
  &Wrapper::Init,
  &Wrapper::Finish,
  nullptr,
  nullptr,
  &Wrapper::Open,
  &Wrapper::Close,
  &Wrapper::Delay,
  nullptr,
  &Wrapper::Play,
  nullptr,
  &Wrapper::Cancel,
  nullptr,
  nullptr,
};
