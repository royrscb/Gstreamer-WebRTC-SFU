#include <gst/gst.h>

int main(int argc, char *argv[]) {
  GstElement *pipeline, *source, *sink, *vertic, *conv;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Create the elements */
  source = gst_element_factory_make ("videotestsrc", "source");
  vertic = gst_element_factory_make("vertigotv","vertic");
  conv = gst_element_factory_make("videoconvert","conv");
  sink = gst_element_factory_make ("autovideosink", "sink");

  /* Create the empty pipeline */
  pipeline = gst_pipeline_new ("test-pipeline");

  /* Build the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, vertic, conv, sink, NULL);
  gst_element_link(source, vertic);
  gst_element_link(vertic, conv);
  gst_element_link(conv, sink);

  /* Modify the source's properties */
  g_object_set (source, "pattern", 0, NULL);

  /* Start playing */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  /* Parse message */
  if (msg != NULL) {
	    GError *err;
	    gchar *debug_info;

	    switch (GST_MESSAGE_TYPE (msg)) {
	      case GST_MESSAGE_ERROR:
	        gst_message_parse_error (msg, &err, &debug_info);
	        g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
	        g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
	        g_clear_error (&err);
	        g_free (debug_info);
	        break;
	      case GST_MESSAGE_EOS:
	        g_print ("End-Of-Stream reached.\n");
	        break;
	      default:
	        /* We should not reach here because we only asked for ERRORs and EOS */
	        g_printerr ("Unexpected message received.\n");
	        break;
	    }
	    gst_message_unref (msg);
  }

  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}
