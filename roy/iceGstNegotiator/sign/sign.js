//Roy Ros Cobo

//---------REQUIRES--------------
const WebSocket = require('ws');//web socket


var port = 3434;

const wss = new WebSocket.Server({ port: port });
console.log("New server on port "+port);


var sockets = [], ids=1;
//this signalling server id is -1
//id of gst server is allways 0
var socket_server = {socket:null, ip: null, id:0};


//--------------txt processing---------------------------------------------
process.openStdin().addListener("data", function(d) {

	var txt = d.toString().trim();

	if(txt==""){
		console.log("LIST OF SOCKETS ("+Object.keys(sockets).length+")\n------------------------------");
    console.log(socket_server.id+" = "+socket_server.ip+" (socket gst server)");
		Object.keys(sockets).forEach(function(key){ console.log(key+" = "+sockets[key].ip); }); 

	}else if("0"<=txt[0] && txt[0]<="9") {

		if("0"<=txt[1] && txt[1]<="9") wss.sendTo(txt[0]+txt[1],protofy("~~~From sign server: "+txt.substring(2,txt.length),txt[0]+txt[1]));
    else wss.sendTo(txt[0],protofy("~~~From sign server: "+txt.substring(1,txt.length),txt[0]));

	}else wss.broadcast("~~~From sign server: "+txt);

});


//----------------------Connections handle------------------
wss.on('connection', function(socket, req) {

  var ip = req.connection.remoteAddress;
  var id = ids; ids++;
  sockets[id]={socket:socket, ip:ip, id:id};

  console.log("^^^Conected "+id+" = "+ip);
  socket.send(protofy("Your id is "+id, id));
  wss.broadcast("^^^Conected "+id+" = "+ip);


  socket.on('message', function(msg) {
      
    var data = JSON.parse(msg);

    console.log("<<<From:"+data.from+" to:"+data.to+" type:"+data.type+" data: "+data.data);
    wss.sendTo(data.to, msg);
  });


  socket.on('close', function(){

  	console.log("vvvDisconected "+id+"="+ip);
  	wss.broadcast("vvvDisconected "+id+"="+ip);
  	delete sockets[id];
  })

});



//----------Private Functions------------------
//entry normal text
wss.broadcast = function(data) {
  wss.clients.forEach(function (client) {
    if (client.readyState === WebSocket.OPEN) {
      
      //to -2 means broadcast
      client.send(protofy(data, -2));
    }
  });
  console.log(")))Broadcast: "+data);
};


//entry a string inside the protocol already stringified
wss.sendTo = function(id,msg){

  if(id>=0 && sockets[id]!=undefined){

    var data = JSON.parse(msg);

    console.log(">>>From:"+data.from+" to:"+data.to+" type:"+data.type+" data: "+data.data);

    sockets[id].socket.send(msg);

  }else if(id==(-2)) wss.broadcast(JSON.parse(msg).data)
}


//intern function to send txt from signalling server
function protofy(txt,to){

  return JSON.stringify({type:"txt", data:txt, from:-1, to:to});
}