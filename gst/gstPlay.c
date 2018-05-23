/*
 * Author: Roy Ros Cobo
 *
 * $ gcc gstPlay.c $(pkg-config --cflags --libs gstreamer-webrtc-1.0 gstreamer-sdp-1.0 libsoup-2.4 json-glib-1.0) -o gstPlay
*/

#include <gst/gst.h>
#include <gst/sdp/sdp.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <string.h>



/////// Structs //////////////////////////////////////////////////////////////////
struct Wrbin{

  GstElement *wrbin;

  GstPad *sinkPad;
  GstPad *srcPad;

  gint ownerPeer;   //id of the peer who owns this wrbin
  gint ownerPipe; //id of the peer who own the pipe where this wrbin is

  gboolean negotiated;
};

struct Peer{

  gint id;

  GstElement *pipel;

  gint nwrbins;
  struct Wrbin wrbins[16];
  
};


typedef struct UserData{

  gint peerID;

  gint index;

} userData;


/////// Constants ////////////////////////////////////////////////////////////////
const gchar* url_sign_server = "wss://127.0.0.1:3434";
const gint MAX_PEERS=8;
const gint MAX_WRBINS=16;

#define VIDEO_COD "vp8enc ! rtpvp8pay ! queue ! application/x-rtp,media=video,payload=96,encoding-name=VP8"
#define AUDIO_COD "opusenc ! rtpopuspay ! queue ! application/x-rtp,media=audio,payload=97,encoding-name=OPUS"

// Because callbacks needs pointers 
userData  
          constUsDa10={ .peerID=1, .index=0 }, constUsDa11={ .peerID=1, .index=1 }, constUsDa12={ .peerID=1, .index=2 }, constUsDa13={.peerID=1, .index=3},
          constUsDa20={ .peerID=2, .index=0 }, constUsDa21={ .peerID=2, .index=1 }, constUsDa22={ .peerID=2, .index=2 }, constUsDa23={.peerID=2, .index=3},
          constUsDa30={ .peerID=3, .index=0 }, constUsDa31={ .peerID=3, .index=1 }, constUsDa32={ .peerID=3, .index=2 }, constUsDa33={.peerID=3, .index=3},
          constUsDa40={ .peerID=4, .index=0 }, constUsDa41={ .peerID=4, .index=1 }, constUsDa42={ .peerID=4, .index=2 }, constUsDa43={.peerID=4, .index=3};

userData* getConstUsDa(gint peerID, gint index){

  switch(peerID){

    case 1: switch(index){

      case 0: return &constUsDa10;
      case 1: return &constUsDa11;
      case 2: return &constUsDa12;
      case 3: return &constUsDa13;
    }

    case 2: switch(index){

      case 0: return &constUsDa20;
      case 1: return &constUsDa21;
      case 2: return &constUsDa22;
      case 3: return &constUsDa23;
    }
    
    case 3: switch(index){

      case 0: return &constUsDa30;
      case 1: return &constUsDa31;
      case 2: return &constUsDa32;
      case 3: return &constUsDa33;
    }

    case 4: switch(index){

      case 0: return &constUsDa40;
      case 1: return &constUsDa41;
      case 2: return &constUsDa42;
      case 3: return &constUsDa43;
    }
  }

  g_print("Error requesting usDa(%i, %i)",peerID, index);
}


//////// Variables ///////////////////////////////////////////////////////////////

static GMainLoop *loop = NULL;
static SoupWebsocketConnection *ws_conn = NULL;

gint npeers=0;
struct Peer peers[8];



static void init_peers(int id){

  int i, times = MAX_PEERS;

  if(id > 0) times=id+1; 

  for(i=id; i<times; i++){ 

    peers[i].id = 99; 
    if(id > 0) gst_object_unref(peers[i].pipel);
    peers[i].pipel = NULL;
    peers[i].nwrbins = 0; 

    int j;
    for(j=0; j<MAX_WRBINS; j++){

      peers[i].wrbins[j].wrbin = NULL;

      peers[i].wrbins[j].sinkPad = NULL;
      peers[i].wrbins[j].srcPad = NULL;

      peers[i].wrbins[j].ownerPeer = 99;
      peers[i].wrbins[j].ownerPipe = 99;

      peers[i].wrbins[j].negotiated = FALSE;
    }
  }
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


static void send_data_to(gchar *type, JsonObject *dataData, gint to, gint index){

  JsonObject *data;

  data = json_object_new ();
  json_object_set_string_member (data, "type", type);
  json_object_set_int_member (data, "index", index);
  json_object_set_object_member (data, "data", dataData);
  json_object_set_int_member (data, "from", 0);
  json_object_set_int_member (data, "to", to);

  soup_websocket_connection_send_text(ws_conn, json_stringify(data));

  g_print(">>> Type:%s to:%i index:%i\n",type, to, index);
}


///////////// Negotiation ///////////////////////////////////////////////////////////////////////

static void send_ice_candidate(GstElement * webrtc, guint mlineindex, gchar * candidate, userData *usDa){

  //g_print("Send candidate to peer:%i for webrtcbin:%i\n", usDa->peerID, usDa->index);

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
  g_print ("Setting local desc(offer) and sending created offer\n");

  g_signal_emit_by_name (peers[usDa->peerID].wrbins[usDa->index].wrbin, "set-local-description", offer, NULL);

  JsonObject *data = json_object_new();
  json_object_set_string_member(data, "type", "offer");
  json_object_set_string_member(data, "sdp", sdp);
  g_free(sdp);

  send_data_to("offer", data, usDa->peerID, usDa->index);

  json_object_unref(data);
  gst_webrtc_session_description_free (offer);
}


static void on_answer_created(GstPromise * promise, userData *usDa){

  GstWebRTCSessionDescription *answer = NULL;
  const GstStructure *reply;

  gst_promise_wait (promise);

  reply = gst_promise_get_reply (promise);
  gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
  gst_promise_unref(promise);


  gchar *sdp = gst_sdp_message_as_text(answer->sdp);
  g_print ("Setting local desc(answer) and sending created answer:\n");

  g_signal_emit_by_name (peers[usDa->peerID].wrbins[usDa->index].wrbin , "set-local-description", answer, NULL);


  JsonObject *data = json_object_new();
  json_object_set_string_member(data, "type", "answer");
  json_object_set_string_member(data, "sdp", sdp);
  g_free(sdp);

  send_data_to("answer", data, usDa->peerID, usDa->index);

  json_object_unref(data);
  gst_webrtc_session_description_free(answer);
}

 
static void negotiate(GstPromise *promise, userData *usDa){

  if(peers[usDa->peerID].wrbins[usDa->index].negotiated) g_print("Negotiation with:%i for webrtcbin:%i already done\n", usDa->peerID, usDa->index);
  else{

    g_print("Negotiate with:%i for webrtcbin:%i\n", usDa->peerID, usDa->index);


    GstPromise *prom;

    prom = gst_promise_new_with_change_func((GstPromiseChangeFunc) on_offer_created, usDa, NULL);
    g_signal_emit_by_name (peers[usDa->peerID].wrbins[usDa->index].wrbin, "create-offer", NULL, prom);
  }
}



////////// PADS ////////////////////////////////////////////////////////////////////

static void set_wrbin_pads(userData *usDa);

static void fake_link_srcpad(GstElement *webrtc, GstPad *new_pad, userData *usDa){

  if (GST_PAD_DIRECTION (new_pad) != GST_PAD_SRC) return;
  else peers[usDa->peerID].wrbins[usDa->index].srcPad = new_pad;


  GstElement *fakesink = gst_element_factory_make("fakesink", NULL);

  gst_bin_add(GST_BIN (peers[ peers[usDa->peerID].wrbins[usDa->index].ownerPipe ].pipel), fakesink);
  GstPad *sinkPad = gst_element_get_static_pad(fakesink, "sink");

  gst_pad_link(new_pad, sinkPad);

  gst_element_sync_state_with_parent(fakesink);
}

static void play_from_srcpad(GstElement *webrtc, GstPad *new_pad, userData *usDa){


  if (GST_PAD_DIRECTION (new_pad) != GST_PAD_SRC) return;
  else peers[usDa->peerID].wrbins[usDa->index].srcPad = new_pad;

  GstElement *outBin = gst_parse_bin_from_description ("rtpvp8depay ! vp8dec ! videoconvert ! queue ! autovideosink", TRUE, NULL);
  gst_bin_add(GST_BIN (peers[usDa->peerID].pipel), outBin);

  GstPad *sinkPad = outBin->sinkpads->data;
  gst_pad_link (new_pad, sinkPad);

  g_print("Creating window to display stream from peer %i in the SFU server", usDa->peerID);

  gst_element_sync_state_with_parent(outBin);
}


static void on_pad_added(GstElement *webrtc, GstPad *new_pad, userData *usDa){

  const gchar *new_pad_type = gst_structure_get_name(gst_caps_get_structure (gst_pad_query_caps (new_pad, NULL), 0));

  g_print("[[[ Pad type:%s added. From:%i index: %i\n", new_pad_type, usDa->peerID, usDa->index);


  play_from_srcpad(webrtc, new_pad, usDa);
}



static void set_wrbin_pads(userData *usDa){

  g_signal_connect(peers[usDa->peerID].wrbins[usDa->index].wrbin, "on-ice-candidate", G_CALLBACK (send_ice_candidate), usDa);
  g_signal_connect(peers[usDa->peerID].wrbins[usDa->index].wrbin, "pad-added", G_CALLBACK (on_pad_added), usDa);
  g_signal_connect(peers[usDa->peerID].wrbins[usDa->index].wrbin, "on-negotiation-needed", G_CALLBACK (negotiate), usDa);
}

/////// Pipeline ///////////

static void start_pipeline(userData *usDa){

  GError *error = NULL;

  GstElement * new_pipe = gst_parse_launch ("videotestsrc pattern=pinwheel ! queue ! "VIDEO_COD" ! webrtcbin name=newBin ", &error);
  if (error) { g_printerr("Failed to parse launch: %s\n", error->message); g_error_free(error); }
    
  peers[usDa->peerID].pipel = new_pipe;
  peers[usDa->peerID].wrbins[usDa->index].wrbin = gst_bin_get_by_name(GST_BIN (new_pipe), "newBin");
  peers[usDa->peerID].wrbins[usDa->index].ownerPeer = usDa->peerID;
  peers[usDa->peerID].wrbins[usDa->index].ownerPipe = usDa->peerID;
  peers[usDa->peerID].nwrbins++;

  set_wrbin_pads(usDa);

  g_print ("Starting pipeline for peer: %i\n", usDa->peerID);
  gst_element_set_state (GST_ELEMENT (new_pipe), GST_STATE_PLAYING);
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
  gint index = 99;
  if(json_object_has_member (data, "index")) index = json_object_get_int_member(data,"index");


  g_print("--------------------------------------------------------\n");
  g_print("<<< Type:%s from:%i to:%i index:%i\n",type,from,to,index);

  if(g_strcmp0(type,"txt")==0) g_print("%s\n", json_object_get_string_member(data,"data"));
  else if(g_strcmp0(type, "socketON")==0){

    JsonObject *data_data = json_object_get_object_member(data, "data");
    const gint id = json_object_get_int_member(data_data, "id");
    const gchar *ip = json_object_get_string_member(data_data, "ip");

    g_print("^^^ New conected %i = %s\n",id,ip);


    // userData *usDa = getConstUsDa(id, 0);

    // peers[id].id = id; //init peer
    // npeers++;
   
    // start_pipeline(usDa);

  }else if(g_strcmp0(type, "socketOFF")==0){

    JsonObject *data_data = json_object_get_object_member(data, "data");
    const gint id = json_object_get_int_member(data_data, "id");
    const gchar *ip = json_object_get_string_member(data_data, "ip");

    init_peers(id);

    if(npeers>0) npeers--;


    g_print("vvv Disconected %i = %s\n",id,ip);

  }else if(g_strcmp0(type, "offer")==0){

    if(peers[from].id != 99) g_print("Offer from peer:%i with index:%i but peer has already registered!", from, index);
    else{

      JsonObject *of = json_object_get_object_member(data, "data");
      const gchar *sdpText = json_object_get_string_member(of, "sdp");

      userData *usDa = getConstUsDa(from, index); //this index must be 0

      peers[from].id = from; //init peer
      npeers++;

      peers[from].wrbins[index].negotiated = TRUE;
      start_pipeline(usDa);

      
      GstSDPMessage *sdp;
      gst_sdp_message_new(&sdp);
      gst_sdp_message_parse_buffer(sdpText, strlen(sdpText), sdp);

      GstWebRTCSessionDescription *offer;
      offer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdp);
      g_signal_emit_by_name (peers[from].wrbins[index].wrbin, "set-remote-description", offer, NULL);

      
      //creating the answer
      GstPromise *promise;
    
      promise = gst_promise_new_with_change_func((GstPromiseChangeFunc) on_answer_created, usDa, NULL);
      g_signal_emit_by_name (peers[from].wrbins[index].wrbin , "create-answer", NULL, promise);
    }
        
  }else if(g_strcmp0(type, "answer")==0){

    JsonObject *ans = json_object_get_object_member(data, "data");
    const gchar *sdpText = json_object_get_string_member(ans, "sdp");


    GstSDPMessage *sdp;
    gst_sdp_message_new(&sdp);
    gst_sdp_message_parse_buffer(sdpText, strlen(sdpText), sdp);

    GstWebRTCSessionDescription *answer;
    answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp);

    g_signal_emit_by_name (peers[from].wrbins[index].wrbin, "set-remote-description", answer, NULL);

  }else if(g_strcmp0(type, "candidate")==0){

    JsonObject *ice = json_object_get_object_member(data, "data");
    const gchar *candidate = json_object_get_string_member(ice, "candidate");
    //const gchar *sdpMid = json_object_get_string_member(ice, "sdpMid");
    gint sdpMLineIndex = json_object_get_int_member(ice, "sdpMLineIndex");

    //g_print("%s\n", candidate);

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
  init_peers(0);
  g_print("Gst Server running\n");

  connect_webSocket_signServer();

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);





  //exit functions

  init_peers(0);
  g_object_unref(ws_conn);
  g_main_loop_unref(loop);

  return 0;
}
