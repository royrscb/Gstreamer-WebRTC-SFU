//Roy Ros Cobo

//---------REQUIRES--------------
const WebSocket = require('ws');


var port = 3434;

const webServerSocket = new WebSocket.Server({ port: port });
console.log("New server on port "+port);


var sockets = [];

//--------------txt processing----------------------------
process.openStdin().addListener("data", function(d) {

	var txt = d.toString().trim();

	if(txt==""){
		console.log("LIST OF SOCKETS\n------------------------------");
		for (var i = 0; i < sockets.length; i++) console.log(i+": "+sockets[i].ip);

	}else if("0"<=txt[0] && txt[0]<="9") sockets[txt[0]].ws.send("~~~From sign server: "+txt.substring(1,txt.length)); 
	else webServerSocket.broadcast("~~~From sign server: "+txt);

});


//----------------------Connections handle------------------
webServerSocket.on('connection', function(ws, req) {

  var ip = req.connection.remoteAddress;
  sockets.push({ws:ws, ip:ip})

  console.log("^^^Conected "+ ip);
  webServerSocket.broadcast("^^^Conected "+ ip);

    ws.on('message', function(message) {
        
        console.log("<<<Message from "+ip+" :"+message);
        webServerSocket.broadcast("<<<Message from "+ip+" :"+message);
    });
});



//----------Functions------------------
webServerSocket.broadcast = function broadcast(data) {
  webServerSocket.clients.forEach(function each(client) {
    if (client.readyState === WebSocket.OPEN) {
      
      client.send(data);
    }
  });
};