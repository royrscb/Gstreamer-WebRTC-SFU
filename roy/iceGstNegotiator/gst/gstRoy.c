//Roy Ros Cobo

#include <gst/gst.h>
#include <gst/sdp/sdp.h>

#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

/* For signalling */
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#include <string.h>


const char* url_sign_server = "wss://127.0.0.1:3434";

GMainLoop *loop = NULL;





static gchar* get_string_from_json_object (JsonObject * object){

  JsonNode *root;
  JsonGenerator *generator;
  gchar *text;

  /* Make it the root node */
  root = json_node_init_object (json_node_alloc (), object);
  generator = json_generator_new ();
  json_generator_set_root (generator, root);
  text = json_generator_to_data (generator, NULL);

  /* Release everything */
  g_object_unref (generator);
  json_node_free (root);

  return text;
}


///////Websocket to signalling server connection///////////////////////////////////////////////////////////

static void on_message(SoupWebsocketConnection *ws, SoupWebsocketDataType type, GBytes *message, gpointer user_data){

  const char *contents;
  gsize len;

  if (type == SOUP_WEBSOCKET_DATA_TEXT) {

    contents = g_bytes_get_data (message, &len);
    g_print("%s\n", contents);
  }

  g_bytes_unref(message);
}

static void on_closed (SoupWebsocketConnection *ws, gpointer user_data){

  g_print("websocket closed\n");
  g_main_loop_quit(loop);
}


static void on_sign_server_connected(GObject *object, GAsyncResult *result, gpointer user_data){

  SoupWebsocketConnection *ws_conn;
  GError *error = NULL;

  ws_conn = soup_session_websocket_connect_finish(SOUP_SESSION(object), result, &error);


  g_signal_connect(ws_conn, "closed",  G_CALLBACK(on_closed),  NULL);
  g_signal_connect(ws_conn, "message", G_CALLBACK(on_message), NULL);

  soup_websocket_connection_send_text(ws_conn, "WebSocket rocks");
}

static gboolean connect_webSocket_signServer(){

  SoupSession *session;
  SoupMessage *msg;
  GError *error = NULL;

  session = soup_session_new();
  g_object_set(G_OBJECT(session), SOUP_SESSION_SSL_STRICT, FALSE, NULL);
 
  msg = soup_message_new(SOUP_METHOD_GET, url_sign_server);

  soup_session_websocket_connect_async(session, msg, NULL, (char **)NULL, NULL, on_sign_server_connected, NULL);


	return TRUE;
}

int main(void){




  /*loop = g_main_loop_new (NULL, FALSE);


  connect_webSocket_signServer();

  g_main_loop_run (loop);


  g_main_loop_unref(loop);*/

  return 0;
}