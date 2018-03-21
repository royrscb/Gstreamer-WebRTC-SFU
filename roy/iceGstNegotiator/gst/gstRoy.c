//Roy Ros Cobo

#include <gst/gst.h>
#include <gst/sdp/sdp.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

/* For signalling */
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <string.h>


//////// Variables ///////////////////////////////////////////////////////////////
const char* url_sign_server = "wss://127.0.0.1:3434";


static GstElement *pipe1, *webrtc1;
static GMainLoop *loop = NULL;
static SoupWebsocketConnection *ws_conn = NULL;

static gint remoteIDtmp = 1;




//////////// JSON functions ///////////////////////////////////////////////////////////////////

static gchar* json_stringify (JsonObject * object){

  JsonNode *root;
  JsonGenerator *generator;
  gchar *msg;

  /* Make it the root node */
  root = json_node_init_object (json_node_alloc (), object);
  generator = json_generator_new ();
  json_generator_set_root (generator, root);
  msg = json_generator_to_data (generator, NULL);

  /* Release everything */
  g_object_unref (generator);
  json_node_free (root);

  return msg;
}

static JsonObject* json_parse(gchar *msg){

  JsonNode *root;
  JsonObject *object;
  JsonParser *parser = json_parser_new ();

  if (!json_parser_load_from_data (parser, msg, -1, NULL))   { g_printerr ("Unknown message '%s', ignoring", msg); g_object_unref (parser);}
  root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root))                         { g_printerr ("Unknown json message '%s', ignoring", msg); g_object_unref (parser);}
  object = json_node_get_object (root);


  return object;
}


void send_data_to(gchar *type, JsonObject *dataData, gint to){

  JsonObject *data;

  data = json_object_new ();
  json_object_set_string_member (data, "type", type);
  json_object_set_object_member (data, "data", dataData);
  json_object_set_int_member (data, "from", 0);
  json_object_set_int_member (data, "to", to);

  soup_websocket_connection_send_text(ws_conn, json_stringify(data));
}


////////// De propina ////////////////////////////////////////////////////////////////////

static void handle_media_stream (GstPad * pad, GstElement * pipe, const char * convert_name, const char * sink_name){

  GstPad *qpad;
  GstElement *q, *conv, *sink;
  GstPadLinkReturn ret;

  g_print ("Trying to handle stream with %s ! %s", convert_name, sink_name);

  q = gst_element_factory_make ("queue", NULL);
  g_assert_nonnull (q);
  conv = gst_element_factory_make (convert_name, NULL);
  g_assert_nonnull (conv);
  sink = gst_element_factory_make (sink_name, NULL);
  g_assert_nonnull (sink);
  gst_bin_add_many (GST_BIN (pipe), q, conv, sink, NULL);
  gst_element_sync_state_with_parent (q);
  gst_element_sync_state_with_parent (conv);
  gst_element_sync_state_with_parent (sink);
  gst_element_link_many (q, conv, sink, NULL);

  qpad = gst_element_get_static_pad (q, "sink");

  ret = gst_pad_link (pad, qpad);
  g_assert_cmphex (ret, ==, GST_PAD_LINK_OK);
}

static void on_incoming_decodebin_stream (GstElement * decodebin, GstPad * pad, GstElement * pipe){

  GstCaps *caps;
  const gchar *name;

  if (!gst_pad_has_current_caps (pad)) {
    g_printerr ("Pad '%s' has no caps, can't do anything, ignoring\n", GST_PAD_NAME (pad));
    return;
  }

  caps = gst_pad_get_current_caps (pad);
  name = gst_structure_get_name (gst_caps_get_structure (caps, 0));

  if (g_str_has_prefix (name, "video"))  handle_media_stream (pad, pipe, "videoconvert", "autovideosink");
  else if (g_str_has_prefix (name, "audio")) handle_media_stream (pad, pipe, "audioconvert", "autoaudiosink");
  else g_printerr ("Unknown pad %s, ignoring", GST_PAD_NAME (pad));
  
}

static void on_incoming_stream (GstElement * webrtc, GstPad * pad, GstElement * pipe){

  GstElement *decodebin;

  if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC) return;

  decodebin = gst_element_factory_make ("decodebin", NULL);
  g_signal_connect (decodebin, "pad-added", G_CALLBACK (on_incoming_decodebin_stream), pipe);
  gst_bin_add (GST_BIN (pipe), decodebin);
  gst_element_sync_state_with_parent (decodebin);
  gst_element_link (webrtc, decodebin);
}


///////////// Negotiation ///////////////////////////////////////////////////////////////////////

static void send_ice_candidate (GstElement * webrtc G_GNUC_UNUSED, guint mlineindex, gchar * candidate, gpointer user_data G_GNUC_UNUSED){

  JsonObject *ice = json_object_new ();

  json_object_set_string_member(ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);

  send_data_to("candidate", ice, remoteIDtmp);
}

static void on_offer_created (GstPromise * promise, gpointer user_data){

  GstWebRTCSessionDescription *offer = NULL;
  const GstStructure *reply;

  gst_promise_wait (promise);

  reply = gst_promise_get_reply (promise);
  gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
  gst_promise_unref(promise);


  gchar *sdp = gst_sdp_message_as_text (offer->sdp);
  g_print ("Setting local desc(offer) and sending created offer:\n%s\n", sdp);

  g_signal_emit_by_name (webrtc1, "set-local-description", offer, NULL);


  JsonObject *data = json_object_new();
  json_object_set_string_member(data, "type", "offer");
  json_object_set_string_member(data, "sdp", sdp);
  g_free(sdp);

  send_data_to("offer", data, remoteIDtmp);

  json_object_unref(data);
  gst_webrtc_session_description_free (offer);
}

static void on_answer_created (GstPromise * promise, gpointer user_data){

  GstWebRTCSessionDescription *answer = NULL;
  const GstStructure *reply;

  gst_promise_wait (promise);

  reply = gst_promise_get_reply (promise);
  gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
  gst_promise_unref(promise);


  gchar *sdp = gst_sdp_message_as_text(answer->sdp);
  g_print ("Setting local desc(answer) and sending created answer:\n%s\n", sdp);

  g_signal_emit_by_name (webrtc1, "set-local-description", answer, NULL);


  JsonObject *data = json_object_new();
  json_object_set_string_member(data, "type", "answer");
  json_object_set_string_member(data, "sdp", sdp);
  g_free(sdp);

  send_data_to("answer", data, remoteIDtmp);

  json_object_unref(data);
  gst_webrtc_session_description_free(answer);
}


static void on_negotiation_needed (GstElement * wrbin, gpointer user_data){

  GstPromise *promise;

  promise = gst_promise_new_with_change_func (on_offer_created, user_data, NULL);
  g_signal_emit_by_name (wrbin, "create-offer", NULL, promise);
}


/////// Pipeline ///////////////////////

static void start_pipeline(){

  GError *error = NULL;

  pipe1 = gst_parse_launch ("videotestsrc ! queue ! vp8enc ! rtpvp8pay ! queue ! "
        "application/x-rtp,media=video,payload=96,encoding-name=VP8 ! webrtcbin name=sendrecv", &error);

  if (error) { g_printerr ("Failed to parse launch: %s\n", error->message); g_error_free (error); if (pipe1) g_clear_object (&pipe1);}

  webrtc1 = gst_bin_get_by_name (GST_BIN (pipe1), "sendrecv");


  //This is the gstwebrtc entry point where we create the offer and so on. It will be called when the pipeline goes to PLAYING.
  g_signal_connect(webrtc1, "on-negotiation-needed", G_CALLBACK (on_negotiation_needed), NULL);
  g_signal_connect(webrtc1, "on-ice-candidate", G_CALLBACK (send_ice_candidate), NULL);
  /* Incoming streams will be exposed via this signal */
  g_signal_connect(webrtc1, "pad-added", G_CALLBACK (on_incoming_stream), pipe1);
  /* Lifetime is the same as the pipeline itself */
  gst_object_unref(webrtc1);

  g_print ("Starting pipeline\n");
  gst_element_set_state (GST_ELEMENT (pipe1), GST_STATE_PLAYING);
}


/////// Signalling server connection ///////////////////////////////////////////////////////////

static void on_sign_message(SoupWebsocketConnection *ws_conn, SoupWebsocketDataType dataType, GBytes *message, gpointer user_data){

  //--------------get msg-------------
  gsize size;
  gchar *msg, *mssg;

  mssg = g_bytes_unref_to_data (message, &size);
  /* Convert to NULL-terminated string */
  msg = g_strndup (mssg, size);
  g_free (mssg);
  
  JsonObject *data = json_parse(msg);

  //---------handle data-------------------

  const gchar *type = json_object_get_string_member(data,"type");
  const gint from = json_object_get_int_member(data,"from");
  const gint to = json_object_get_int_member(data,"to");


  g_print("--------------------------------------------------------\n");
  g_print("<<< Type:%s from:%i to:%i\n",type,from,to);

  if(g_strcmp0(type,"txt")==0) g_print("%s\n", json_object_get_string_member(data,"data"));
  else if(g_strcmp0(type, "socketON")==0){

    JsonObject *data_data = json_object_get_object_member(data, "data");
    const gint id = json_object_get_int_member(data_data, "id");
    const gchar *ip = json_object_get_string_member(data_data, "ip");

    g_print("^^^ New conected %i = %s\n",id,ip);
    g_print("Connecting with him");

    remoteIDtmp = id;
    start_pipeline();

  }else if(g_strcmp0(type, "socketOFF")==0){

    JsonObject *data_data = json_object_get_object_member(data, "data");
    const gint id = json_object_get_int_member(data_data, "id");
    const gchar *ip = json_object_get_string_member(data_data, "ip");

    g_print("vvv Disconected %i = %s\n",id,ip);

  }else if(g_strcmp0(type, "offer")==0){

    JsonObject *of = json_object_get_object_member(data, "data");
    const gchar *sdpText = json_object_get_string_member(of, "sdp");

    g_print("OFFER received: %s\n", json_stringify(of));

    GstSDPMessage *sdp;
    gst_sdp_message_new(&sdp);
    gst_sdp_message_parse_buffer(sdpText, strlen(sdpText), sdp);

    GstWebRTCSessionDescription *offer;
    offer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdp);
    g_signal_emit_by_name (webrtc1, "set-remote-description", offer, NULL);

    
    //creating the answer
    GstPromise *promise;
  
    promise = gst_promise_new_with_change_func(on_answer_created, user_data, NULL);
    g_signal_emit_by_name (webrtc1, "create-answer", NULL, promise);
    
  }else if(g_strcmp0(type, "answer")==0){

    JsonObject *ans = json_object_get_object_member(data, "data");
    const gchar *sdpText = json_object_get_string_member(ans, "sdp");

    g_print("ANSWER received: %s\n",json_stringify(ans));


    GstSDPMessage *sdp;
    gst_sdp_message_new(&sdp);
    gst_sdp_message_parse_buffer(sdpText, strlen(sdpText), sdp);

    GstWebRTCSessionDescription *answer;
    answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp);
    g_signal_emit_by_name (webrtc1, "set-remote-description", answer, NULL);

  }else if(g_strcmp0(type, "candidate")==0){

    JsonObject *ice = json_object_get_object_member(data, "data");
    const gchar *candidate = json_object_get_string_member(ice, "candidate");
    //const gchar *sdpMid = json_object_get_string_member(ice, "sdpMid");
    gint sdpMLineIndex = json_object_get_int_member(ice, "sdpMLineIndex");

    g_print("Candidate received: %s\n", json_stringify(ice));
    g_signal_emit_by_name (webrtc1, "add-ice-candidate", sdpMLineIndex, candidate);

  }else g_print("Unknown type! %s\n",msg);


  g_free(msg);
}

static void on_sign_closed (SoupWebsocketConnection *ws_conn, gpointer user_data){

  g_print("websocket closed\n");
  g_main_loop_quit(loop);
}


static void on_sign_server_connected(GObject *object, GAsyncResult *result, gpointer user_data){

  ws_conn = soup_session_websocket_connect_finish(SOUP_SESSION(object), result, NULL);

  g_signal_connect(ws_conn, "message", G_CALLBACK(on_sign_message), NULL);
  g_signal_connect(ws_conn, "closed",  G_CALLBACK(on_sign_closed),  NULL); 


  g_print("Connected to the sign server succesfully\n");
}

static void connect_webSocket_signServer(){

  SoupSession *session;
  SoupMessage *msg;

  session = soup_session_new();
  g_object_set(G_OBJECT(session), SOUP_SESSION_SSL_STRICT, FALSE, NULL);
 
  msg = soup_message_new(SOUP_METHOD_GET, url_sign_server);

  soup_session_websocket_connect_async(session, msg, "gstServer", (char **)NULL, NULL, on_sign_server_connected, NULL);
}

int main(int argc, char *argv[]){

  gst_init (&argc, &argv);
  g_print("Gst Server running\n");

  loop = g_main_loop_new (NULL, FALSE);


  connect_webSocket_signServer();

  //start_pipeline();

  g_main_loop_run (loop);








  g_object_unref(ws_conn);
  g_main_loop_unref(loop);

  return 0;
}