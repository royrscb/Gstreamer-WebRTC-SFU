#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <libsoup/soup.h>

static SoupLogger *logger;

static const char *state_names[] = {
    "NEW", 
    "CONNECTING", 
    "IDLE", 
    "IN_USE",
    "REMOTE_DISCONNECTED", 
    "DISCONNECTED"
};

static void connection_state_changed(GObject *object, GAsyncResult *result, gpointer user_data){

  SoupConnectionState *state = user_data;
  SoupConnectionState new_state;

  g_object_get(object, "state", &new_state, NULL);

  g_print("Change state: %s -> %s\n", state_names[*state], state_names[new_state]);

  *state = new_state;
}

static void connection_created(SoupSession *session, GObject *conn, gpointer user_data){

  g_print("conection created");
  SoupConnectionState *state = user_data;

  g_object_get(conn, "state", state, NULL);

  g_signal_connect(conn, "notify::state", G_CALLBACK(connection_state_changed), state);
}

static void on_message(SoupWebsocketConnection *ws, SoupWebsocketDataType type, GBytes *message, gpointer user_data){

  const char *contents;
  gsize len;

  g_print("websocket message received\n");

  if (type == SOUP_WEBSOCKET_DATA_TEXT) {

    contents = g_bytes_get_data (message, &len);
    g_print("%s\n", contents);
  }

  g_bytes_unref(message);
}

static void on_closed (SoupWebsocketConnection *ws, gpointer user_data){

  g_print("websocket closed\n");
}

static void client_connect_complete(GObject *object, GAsyncResult *result, gpointer user_data){

  SoupWebsocketConnection *connection;
  GError *error = NULL;

  connection = soup_session_websocket_connect_finish(SOUP_SESSION(object), result, &error);

  if (connection) {

    g_signal_connect(connection, "closed",  G_CALLBACK(on_closed),  NULL);
    g_signal_connect(connection, "message", G_CALLBACK(on_message), NULL);
    //g_signal_connect(connection, "closing", G_CALLBACK(on_closing_send_message), message);

    soup_websocket_connection_send_text(connection, "WebSocket rocks");
  }
}

static SoupConnectionState state;

static gboolean websocket_start(){

  SoupSession *session;
  SoupMessage *msg;
  GError *error = NULL;

  g_print("state: %d\n", state);

  if (state == SOUP_CONNECTION_NEW || state == SOUP_CONNECTION_DISCONNECTED || state == SOUP_CONNECTION_REMOTE_DISCONNECTED){
    
    session = soup_session_new();
    g_object_set(G_OBJECT(session), SOUP_SESSION_SSL_STRICT, FALSE, NULL);
    soup_session_add_feature(session, SOUP_SESSION_FEATURE(logger));

    g_signal_connect(session, "connection-created", G_CALLBACK(connection_created), &state);

    msg = soup_message_new(SOUP_METHOD_GET, "wss://127.0.0.1:3434");

    soup_session_websocket_connect_async(session, msg, NULL, (char **)NULL, NULL, client_connect_complete, NULL);
  }

	return TRUE;
}

int main(void){

  GMainLoop *loop = NULL;
  logger = soup_logger_new (SOUP_LOGGER_LOG_BODY, -1);

  loop = g_main_loop_new (NULL, FALSE);


  websocket_start();

  g_main_loop_run (loop);


  g_main_loop_unref(loop);

  return EXIT_SUCCESS;
}