/*  
 * Author: Roy Ros Cobo
 *
 * $ gcc gstRoy.c $(pkg-config --cflags --libs gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0) -o gstRoy
*/

#include <gst/gst.h>
#include <gst/sdp/sdp.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <string.h>


/////// Constants ////////////////////////////////////////////////////////////////
#define VIDEO_COD "vp8enc ! rtpvp8pay ! queue ! application/x-rtp,media=video,payload=96,encoding-name=VP8"
#define AUDIO_COD "opusenc ! rtpopuspay ! queue ! application/x-rtp,media=audio,payload=97,encoding-name=OPUS"


//////// Variables ///////////////////////////////////////////////////////////////
const char* url_sign_server = "wss://127.0.0.1:3434";

static GMainLoop *loop = NULL;
static SoupWebsocketConnection *ws_conn = NULL;


struct Wrbin{

  GstElement *wrbin;

  GstPad *sinkPad;
  GstPad *srcPad;

  gboolean connected;

  gint peerOwner;
  gint pipeOwner;
};


struct Peer{

  gint id;

  struct Wrbin wrbins[5];
  gint nwrbins;

};


struct Pipeline{

  GstElement *pipel;
  
  struct Wrbin wrbins[5];
  gint nwrbins;

};

gint npeers=0;
struct Peer peers[10];

gint npipes=0;
struct Pipeline pipes[5];


typedef struct UserData{

  gint peerID;

  gint index;

} userData;


//////// TO REALLY SOLVE //////////////////////////
userData  tmpUsDa10={ .peerID=1, .index=0 },
          tmpUsDa11={ .peerID=1, .index=1 }, 
          tmpUsDa20={ .peerID=2, .index=0 },
          tmpUsDa21={ .peerID=2, .index=1 };

userData* getStaticUsDa(gint peerID, gint index){

  if(peerID == 1){

    if(index == 0) return &tmpUsDa10;
    else if(index == 1) return &tmpUsDa11;

  }else if(peerID == 2){

    if(index == 0) return &tmpUsDa20;
    else if(index == 1) return &tmpUsDa21;
  
  }else g_print("Error requesting usDa");
}

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


void send_data_to(gchar *type, JsonObject *dataData, gint to, gint index){

  JsonObject *data;

  data = json_object_new ();
  json_object_set_string_member (data, "type", type);
  json_object_set_int_member (data, "index", index);
  json_object_set_object_member (data, "data", dataData);
  json_object_set_int_member (data, "from", 0);
  json_object_set_int_member (data, "to", to);

  soup_websocket_connection_send_text(ws_conn, json_stringify(data));

  g_print(">>> Type:%s to:%i index:%i data: \n%s\n",type, to, index, json_stringify(dataData));
}

/////// Pipeline ///////////

GstElement* start_pipeline(gint n){

  GError *error = NULL;

  pipes[n].pipel = gst_parse_launch ("videotestsrc ! queue ! "VIDEO_COD" ! webrtcbin name=newBin ", &error);

  if (error) { g_printerr("Failed to parse launch: %s\n", error->message); g_error_free(error); }


  g_print ("Starting pipeline n: %i\n", n);
  gst_element_set_state (GST_ELEMENT (pipes[n].pipel), GST_STATE_PLAYING);

  return gst_bin_get_by_name(GST_BIN (pipes[n].pipel), "newBin");
}

///////////// Negotiation ///////////////////////////////////////////////////////////////////////

static void send_ice_candidate(GstElement * webrtc G_GNUC_UNUSED, guint mlineindex, gchar * candidate, userData *usDa){

  g_print("Send candidate to peer:%i for webrtcbin:%i\n", usDa->peerID, usDa->index);

  JsonObject *ice = json_object_new ();

  json_object_set_string_member(ice, "candidate", candidate);
  json_object_set_int_member (ice, "sdpMLineIndex", mlineindex);

  send_data_to("candidate", ice, usDa->peerID, usDa->index);
}

static void on_offer_created(GstPromise * promise, userData *usDa){

  g_print("Creating offer with peer:%i for webrctbin index:%i\n", usDa->peerID, usDa->index);
  GstWebRTCSessionDescription *offer = NULL;
  const GstStructure *reply;

  gst_promise_wait (promise);

  reply = gst_promise_get_reply (promise);
  gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
  gst_promise_unref(promise);


  gchar *sdp = gst_sdp_message_as_text (offer->sdp);
  g_print ("Setting local desc(offer) and sending created offer:\n%s\n", sdp);

  g_signal_emit_by_name (peers[usDa->peerID].wrbins[usDa->index].wrbin, "set-local-description", offer, NULL);

  JsonObject *data = json_object_new();
  json_object_set_string_member(data, "type", "offer");
  json_object_set_string_member(data, "sdp", sdp);
  g_free(sdp);

  send_data_to("offer", data, usDa->peerID, usDa->index);

  json_object_unref(data);
  gst_webrtc_session_description_free (offer);
}

/*
static void on_answer_created(GstPromise * promise, gpointer wrbin){

  GstWebRTCSessionDescription *answer = NULL;
  const GstStructure *reply;

  gst_promise_wait (promise);

  reply = gst_promise_get_reply (promise);
  gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
  gst_promise_unref(promise);


  gchar *sdp = gst_sdp_message_as_text(answer->sdp);
  g_print ("Setting local desc(answer) and sending created answer:\n%s\n", sdp);

  g_signal_emit_by_name (wrbin, "set-local-description", answer, NULL);


  JsonObject *data = json_object_new();
  json_object_set_string_member(data, "type", "answer");
  json_object_set_string_member(data, "sdp", sdp);
  g_free(sdp);

  send_data_to("answer", 99, data, 99);

  json_object_unref(data);
  gst_webrtc_session_description_free(answer);
}
*/
 
static void negotiate(userData *usDa){

  g_print("Negotiate with:%i for webrtcbin:%i\n", (*usDa).peerID, usDa->index);


  GstPromise *promise;

  promise = gst_promise_new_with_change_func((GstPromiseChangeFunc) on_offer_created, usDa, NULL);
  g_signal_emit_by_name (peers[usDa->peerID].wrbins[usDa->index].wrbin, "create-offer", NULL, promise);
}



////////// PADS ////////////////////////////////////////////////////////////////////
static void set_wrbin_pads(userData *usDa);

static void play_from_pad(GstElement *webrtc, GstPad *new_pad, userData *usDa){

  g_print("final padd adeddddddddddddddkdkdkddk\n");

  GstElement *out;
  GstPad *sink;

  if (GST_PAD_DIRECTION (new_pad) != GST_PAD_SRC) return;

  out = gst_parse_bin_from_description ("rtpvp8depay ! vp8dec ! videoconvert ! queue ! xvimagesink", TRUE, NULL);
  gst_bin_add (GST_BIN (pipes[ peers[usDa->peerID].wrbins[usDa->index].pipeOwner ].pipel), out);
  gst_element_sync_state_with_parent (out);

  sink = out->sinkpads->data;

  gst_pad_link (new_pad, sink);

}


static void _webrtc_pad_added(GstElement *webrtc, GstPad *new_pad, userData *usDa){

  if(usDa->peerID == 2 && usDa->index == 0){


    if (GST_PAD_DIRECTION (new_pad) != GST_PAD_SRC) return;
    else  peers[usDa->peerID].wrbins[usDa->index].srcPad = new_pad;

    g_print("Pad from:%i addedddddd: %i\n",usDa->peerID, usDa->index);


    gint peer2conn = 1;//primer conectat
    gint pipe2conn = 1;//segona pipe creada


    GstElement *out;
    out=gst_parse_bin_from_description("rtpvp8depay ! vp8dec ! videoconvert ! queue ! "VIDEO_COD" ! webrtcbin name=newBin  ", TRUE, NULL);
    gst_bin_add(GST_BIN (pipes[pipe2conn].pipel), out);
    gst_element_sync_state_with_parent(out);

    GstPad *sink = out->sinkpads->data;

    gst_pad_link (new_pad, sink);



    struct Wrbin new_wrbin;
    new_wrbin.wrbin = gst_bin_get_by_name(GST_BIN (out), "newBin");
    new_wrbin.sinkPad = sink;
    new_wrbin.connected = TRUE;
    new_wrbin.peerOwner=peer2conn;
    new_wrbin.pipeOwner=pipe2conn;

    gint newIndex = peers[peer2conn].nwrbins;

    pipes[pipe2conn].wrbins[ pipes[pipe2conn].nwrbins ] = new_wrbin;
    pipes[pipe2conn].nwrbins++;

    peers[peer2conn].wrbins[newIndex] = new_wrbin;
    peers[peer2conn].nwrbins++;


    userData *usDa2 = getStaticUsDa(peer2conn, newIndex);

    set_wrbin_pads(usDa2);
    
    negotiate(usDa2);
  }
}


static void set_wrbin_pads(userData *usDa){

  g_signal_connect(peers[usDa->peerID].wrbins[usDa->index].wrbin, "on-ice-candidate", G_CALLBACK (send_ice_candidate), usDa);
  g_signal_connect(peers[usDa->peerID].wrbins[usDa->index].wrbin, "pad-added", G_CALLBACK (_webrtc_pad_added), usDa);
  g_signal_connect(peers[usDa->peerID].wrbins[usDa->index].wrbin, "on-negotiation-needed", G_CALLBACK (negotiate), usDa);
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



    struct Wrbin new_wrbin;
    new_wrbin.wrbin = start_pipeline(npipes); npipes++;
    new_wrbin.pipeOwner = npipes -1;
    new_wrbin.connected = TRUE;
    new_wrbin.peerOwner = id;

    pipes[npipes -1].wrbins[pipes[npipes -1].nwrbins] = new_wrbin;
    pipes[npipes -1].nwrbins++; 

    struct Peer new_peer = { .id=id, .wrbins[0]=new_wrbin, .nwrbins=1 };
    peers[id] = new_peer;
    npeers++;

    userData *usDa = getStaticUsDa(id, 0);

    set_wrbin_pads(usDa);

    negotiate(usDa);

  }else if(g_strcmp0(type, "socketOFF")==0){

    JsonObject *data_data = json_object_get_object_member(data, "data");
    const gint id = json_object_get_int_member(data_data, "id");
    const gchar *ip = json_object_get_string_member(data_data, "ip");

    npipes--;
    npeers--;

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
    //*****g_signal_emit_by_name (TO_DO, "set-remote-description", offer, NULL);

    
    //creating the answer
    GstPromise *promise;
  
    //*****promise = gst_promise_new_with_change_func(on_answer_created, user_data, NULL);
    //*****g_signal_emit_by_name (TO_DO, "create-answer", NULL, promise);
        
  }else if(g_strcmp0(type, "answer")==0){

    const gint index = json_object_get_int_member(data,"index");

    JsonObject *ans = json_object_get_object_member(data, "data");
    const gchar *sdpText = json_object_get_string_member(ans, "sdp");

    g_print("ANSWER received: %s\n",json_stringify(ans));


    GstSDPMessage *sdp;
    gst_sdp_message_new(&sdp);
    gst_sdp_message_parse_buffer(sdpText, strlen(sdpText), sdp);

    GstWebRTCSessionDescription *answer;
    answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp);

    g_signal_emit_by_name (peers[from].wrbins[index].wrbin, "set-remote-description", answer, NULL);

  }else if(g_strcmp0(type, "candidate")==0){

    const gint index = json_object_get_int_member(data,"index");

    JsonObject *ice = json_object_get_object_member(data, "data");
    const gchar *candidate = json_object_get_string_member(ice, "candidate");
    //const gchar *sdpMid = json_object_get_string_member(ice, "sdpMid");
    gint sdpMLineIndex = json_object_get_int_member(ice, "sdpMLineIndex");

    g_print("Candidate received: %s\n", json_stringify(ice));

    g_signal_emit_by_name (peers[from].wrbins[index].wrbin, "add-ice-candidate", sdpMLineIndex, candidate);
    

  }else g_print("Unknown type! %s\n",msg);


  g_free(msg);
}

static void on_sign_closed (SoupWebsocketConnection *ws_conn, gpointer user_data){

  g_print("websocket closed\n");
  g_main_loop_quit(loop);
}


static void on_sign_server_connected(GObject *object, GAsyncResult *result, gpointer user_data){

  ws_conn = soup_session_websocket_connect_finish(SOUP_SESSION(object), result, NULL);
  if (soup_websocket_connection_get_state (ws_conn) != SOUP_WEBSOCKET_STATE_OPEN) g_main_loop_quit(loop);

  g_signal_connect(ws_conn, "message", G_CALLBACK(on_sign_message), NULL);
  g_signal_connect(ws_conn, "closed",  G_CALLBACK(on_sign_closed),  NULL); 


  soup_websocket_connection_send_text(ws_conn, "{\"type\":\"txt\",\"data\":\"Im the GST server!\\n\",\"from\":\"0\",\"to\":\"-1\"}");
  g_print("Connected to the sign server succesfully\n");
}

static void connect_webSocket_signServer(){

  SoupSession *session;
  SoupMessage *msg;
  const char *https_aliases[] = {"wss", NULL};

  session = soup_session_new_with_options (SOUP_SESSION_SSL_STRICT, TRUE,
      SOUP_SESSION_SSL_USE_SYSTEM_CA_FILE, TRUE,
      SOUP_SESSION_HTTPS_ALIASES, https_aliases, NULL);
  g_object_set(G_OBJECT(session), SOUP_SESSION_SSL_STRICT, FALSE, NULL);
 
  msg = soup_message_new(SOUP_METHOD_GET, url_sign_server);

  soup_session_websocket_connect_async(session, msg, "gstServer", (char **)NULL, NULL, on_sign_server_connected, NULL);
}


int main(int argc, char *argv[]){

  gst_init (&argc, &argv);
  g_print("Gst Server running\n");

  connect_webSocket_signServer();

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);





  //exit functions

  g_object_unref(ws_conn);
  g_main_loop_unref(loop);

  return 0;
}