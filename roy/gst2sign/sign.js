//Roy Ros Cobo

//---------REQUIRES--------------
const WebSocket = require('ws');


var port = 3434;

const webServerSocket = new WebSocket.Server({ port: port });
console.log("New server on port "+port);


var sockets = [], ids=0;

//--------------txt processing----------------------------
process.openStdin().addListener("data", function(d) {

	var txt = d.toString().trim();

	if(txt==""){
		console.log("LIST OF SOCKETS ("+Object.keys(sockets).length+")");
		console.log("------------------------------");
		Object.keys(sockets).forEach(function(key){ console.log(key+"="+sockets[key].ip); }); 

	}else if("0"<=txt[0] && txt[0]<="9") {

		if("0"<=txt[1] && txt[1]<="9" && sockets[txt[0]+txt[1]]!=undefined) {

			sockets[txt[0]+txt[1]].ws.send("~~~From sign server: "+txt.substring(2,txt.length)); 
			console.log(">>>Sending to "+txt[0]+txt[1]+":"+txt.substring(2,txt.length));

		}else if(sockets[txt[0]]!=undefined) {
			sockets[txt[0]].ws.send("~~~From sign server: "+txt.substring(1,txt.length)); 
			console.log(">>>Sending to "+txt[0]+": "+txt.substring(1,txt.length));
		}

	}else webServerSocket.broadcast("~~~From sign server: "+txt);

});


//----------------------Connections handle------------------
webServerSocket.on('connection', function(ws, req) {

  var ip = req.connection.remoteAddress;
  var id = ids; ids++;
  sockets[id]={ws:ws, ip:ip};

  console.log("^^^Conected "+id+"="+ip);
  ws.send("Your id is "+id);
  webServerSocket.broadcast("^^^Conected "+id+"="+ip);


  ws.on('message', function(message) {
      
      console.log("<<<Message from "+id+": "+message);
      webServerSocket.broadcast(">>>From "+id+": "+message);
  });


  ws.on('close', function(){

  	console.log("vvvDisconected "+id+"="+ip);
  	webServerSocket.broadcast("vvvDisconected "+id+"="+ip);
  	delete sockets[id];
  })

});



//----------Functions------------------
webServerSocket.broadcast = function broadcast(data) {
  webServerSocket.clients.forEach(function (client) {
    if (client.readyState === WebSocket.OPEN) {
      
      client.send(data);
    }
  });
  console.log(")))Broadcast: "+data)
};
