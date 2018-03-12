const WebSocket = require('ws');


var port = 3434;

const wss = new WebSocket.Server({ port: port });

console.log("New server on port "+port);

wss.broadcast = function(data) {
    for(var i in this.clients) {
        this.clients[i].send(data);
    }
};

wss.on('connection', function(ws, req) {

  var ip = req.connection.remoteAddress;

  console.log("new one "+ ip);

    ws.on('message', function(message) {
        console.log('received: %s', message);
        wss.broadcast("From "+ip+" :"+message);
        ws.send("Your msg: "+message);
    });
});







/*


io.sockets.on('connection', function (socket){

  usuarisConnectats.push(socket.id); //s'ha connectat un usuari, socket.id té l'identificador
  console.log("USUARI ID -> " + socket.id + " || CONNECT");
  if (usuarisConnectats.length > 1){ //quan els dos usuaris s'han connectat al server
    
    usuari1 = usuarisConnectats[0];
    usuari2 = usuarisConnectats[1];
    
    io.to(usuari1).emit('message', {type:'newUser', msg:usuari2}); //enviem info de user1 -> user2
    io.to(usuari2).emit('message', {type:'newUser', msg:usuari1}); //enviem info de user2 -> user1
  }
  // data is an object with receiver socket id and the message
  socket.on('message', function (message) {
    console.log(message);
    console.log('-------------------------------------------------------------------------');
    socket.to(message.to).emit('message', message);
  });
  
  socket.on('disconnect', function (message) {
    console.log("USUARI ID -> " + socket.id + " || DISCONNECT");

    if(usuari1 == socket.id) io.to(usuari2).emit('message', {type:'hangup'});
    else io.to(usuari1).emit('message', {type:'hangup'});
    
    usuarisDesconnectats ++;
    
    if(usuarisDesconnectats > 1){
      //tots els usuaris s'han desconnectat
      console.log("DELETE USERS FROM USER LIST");
      usuarisDesconnectats = 0;
      usuarisConnectats = [];
    }
  });
});


*/