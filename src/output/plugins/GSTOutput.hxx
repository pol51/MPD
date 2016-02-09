#pragma once

#include "config.h"
#include "GSTOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "../Wrapper.hxx"
#include "../Timer.hxx"
#include "../../thread/Thread.hxx"

#include <gst/gst.h>

class GSTOutput
{
  public:
    GSTOutput();

    bool Initialize(const ConfigBlock &block, Error &error);
    static GSTOutput *Create(const ConfigBlock &block, Error &error);
    bool Open(AudioFormat &audio_format, Error &error);
    void Close();
    unsigned Delay() const;
    size_t Play(const void *chunk, size_t size, Error &error);
    void Cancel();

    static gboolean GstBusCallback(GstBus *bus, GstMessage *message, gpointer *ptr);

  protected:
    static void GMainLoopRun(void *arg) { ((GSTOutput*)arg)->GMainLoopRun(); }
    void GMainLoopRun();

  protected:
    friend struct AudioOutputWrapper<GSTOutput>;

    AudioOutput base;
    Thread thread;
    Timer *timer;

    GstClockTime timestamp = 0;
    GMainLoop *loop;
    GstElement *pipeline, *appsrc, *audiosink;
};
