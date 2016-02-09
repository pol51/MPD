#include "GSTOutput.hxx"

#include "../../Log.hxx"
#include "../../util/Domain.hxx"
#include "../../thread/Thread.hxx"
#include "../../thread/Name.hxx"
#include "../../thread/Util.hxx"
#include "../../thread/Slack.hxx"

#include <memory.h>

#include <gst/app/gstappsrc.h>
#include <gst/audio/audio-format.h>

static constexpr Domain gstreamer_audio_output_domain("gstreamer_audio_output");

GSTOutput::GSTOutput() :
  base(gst_output_plugin)
{
}

bool GSTOutput::Initialize(const ConfigBlock &block, Error &error)
{
  int argc = 0;
  char **argv = nullptr;
  if (!gst_init_check(&argc, &argv, nullptr))
    return false;

  return base.Configure(block, error);
}

GSTOutput * GSTOutput::Create(const ConfigBlock &block, Error &error)
{
  GSTOutput *nd = new GSTOutput();

  if (!nd->Initialize(block, error))
  {
    delete nd;
    return nullptr;
  }

  return nd;
}

bool GSTOutput::Open(AudioFormat &audio_format, Error &error)
{
  timer = new Timer(audio_format);
  timestamp = 0;

  loop = g_main_loop_new(nullptr, FALSE);

  pipeline = gst_pipeline_new("pipeline");
  appsrc = gst_element_factory_make("appsrc", "appsrc");
  audiosink = gst_element_factory_make("autoaudiosink", "audiosink");

  GstBus *bus = gst_pipeline_get_bus((GstPipeline*)pipeline);
  gst_bus_add_watch(bus, (GstBusFunc)GstBusCallback, this);

  g_object_set(G_OBJECT(appsrc),
               "caps",  gst_caps_new_simple("audio/x-raw",
                                            "channels", G_TYPE_INT, 2,
                                            "rate", G_TYPE_INT, 44100,
                                            "format", G_TYPE_STRING, "F32LE",
                                            "layout", G_TYPE_STRING, "interleaved",
                                            nullptr),
               "stream-type", GST_APP_STREAM_TYPE_STREAM,
               "format", GST_FORMAT_TIME, nullptr);


  gst_bin_add_many(GST_BIN(pipeline), appsrc, audiosink, nullptr);
  gst_element_link_many(appsrc, audiosink, nullptr);

  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  if (!thread.Start(GMainLoopRun, this, error))
    return false;
  return true;
}

void GSTOutput::Close()
{
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(GST_OBJECT(pipeline));
  g_main_loop_unref(loop);
  delete timer;
}

unsigned GSTOutput::Delay() const
{
  return timer->IsStarted()
      ? timer->GetDelay()
      : 0;
}

size_t GSTOutput::Play(const void *chunk, size_t size, Error &error)
{
  if (!timer->IsStarted())
    timer->Start();
  timer->Add(size);

  GstBuffer *buffer = gst_buffer_new_and_alloc(size);
  GstMapInfo info;
  gst_buffer_map(buffer, &info, GST_MAP_READ);
  memcpy(info.data, chunk, size);
  gst_buffer_unmap(buffer, &info);

  GST_BUFFER_PTS(buffer) = timestamp;
  GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(timer->GetDelay(), GST_MSECOND, 2);
  timestamp += GST_BUFFER_DURATION(buffer)/1000;

  gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);

  return size;
}

void GSTOutput::Cancel()
{
  timer->Reset();
}

gboolean GSTOutput::GstBusCallback(GstBus *bus, GstMessage *message, gpointer *ptr)
{
  GSTOutput *app = (GSTOutput*)ptr;

  FormatDebug(gstreamer_audio_output_domain, "New Message on GstBus: %s", gst_message_type_get_name(GST_MESSAGE_TYPE(message)));

  switch (GST_MESSAGE_TYPE(message))
  {
    case GST_MESSAGE_ERROR:
      {
        gchar *debug;
        GError *err;

        gst_message_parse_error(message, &err, &debug);
        FormatDebug(gstreamer_audio_output_domain, "Error %s", err->message);
        g_error_free(err);
        g_free(debug);
        g_main_loop_quit(app->loop);
      }
      break;
    case GST_MESSAGE_WARNING:
      {
        gchar *debug;
        GError *err;

        gst_message_parse_warning(message, &err, &debug);
        FormatDebug(gstreamer_audio_output_domain, "Warning %s", err->message);
        FormatDebug(gstreamer_audio_output_domain, "Debug %s", debug);

        const gchar * name = GST_MESSAGE_SRC_NAME(message);

        FormatDebug(gstreamer_audio_output_domain, "Name of src %s", name ? name : "nil");
        g_error_free(err);
        g_free(debug);
      }
      break;
    case GST_MESSAGE_EOS:
      FormatDebug(gstreamer_audio_output_domain, "End of stream");
      g_main_loop_quit(app->loop);
      break;
    case GST_MESSAGE_STATE_CHANGED:
      break;
    default:
      FormatDebug(gstreamer_audio_output_domain, "got message %s", gst_message_type_get_name(GST_MESSAGE_TYPE(message)));
      break;
  }

  return TRUE;
}

void GSTOutput::GMainLoopRun()
{
  SetThreadName("gst: g_main_loop");

  g_main_loop_run(loop);
}
