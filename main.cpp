#include <nodepp/nodepp.h>
#include <bluetooth/bth.h>

using namespace nodepp;

void onMain() {

    bluetooth_t bth; 
    auto devices = bth.get_devices();
    forEach( x, devices ){ console::log( "->", x ); }
    
    if( devices.empty() ){ 
        console::log("No Devices Found");
        return; 
    }

    auto server = bth::client();

    server.onConnect([=]( bsocket_t cli ){
        console::log("connected");
    });

    server.connect( devices[0], 1, [=]( bsocket_t cli ){
        console::log("listening");
    });

}