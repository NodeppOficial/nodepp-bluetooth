/*
 * Copyright 2023 The Nodepp Project Authors. All Rights Reserved.
 *
 * Licensed under the MIT (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://github.com/NodeppOficial/nodepp/blob/main/LICENSE
 */

/*────────────────────────────────────────────────────────────────────────────*/

#ifndef NODEPP_BLUETOOTH
#define NODEPP_BLUETOOTH

/*────────────────────────────────────────────────────────────────────────────*/

#include "bsocket.h"
#include "poll.h"

/*────────────────────────────────────────────────────────────────────────────*/

namespace nodepp {

/*────────────────────────────────────────────────────────────────────────────*/

class bth_t {
protected:

    struct NODE {
        int                        state = 0;
        bool                       chck  = 1;
        agent_t                    agent;
        poll_t                     poll ;
        function_t<void,bsocket_t> func ;
    };  ptr_t<NODE> obj;
    
    /*─······································································─*/

    void init_poll_loop( ptr_t<const bth_t>& self ) const noexcept { process::poll::add([=](){
        if( self->is_closed() ){ return -1; } if( self->obj->poll.emit() != -1 ) { auto x = self->obj->poll.get_last_poll();
            if( x[0] == 0 ){ bsocket_t cli(x[1]); cli.set_sockopt( self->obj->agent ); self->onSocket.emit(cli); self->obj->func(cli); }
            if( x[0] == 1 ){ bsocket_t cli(x[1]); cli.set_sockopt( self->obj->agent ); self->onSocket.emit(cli); self->obj->func(cli); }
        #if _KERNEL == NODEPP_KERNEL_WINDOWS
            if( x[0] ==-1 ){ ::closesocket(x[1]); }
        #else
            if( x[0] ==-1 ){ ::close(x[1]); }
        #endif
        }   return 1;
    }); }
    
public: bth_t() noexcept : obj( new NODE() ) {}

    event_t<bsocket_t> onConnect;
    event_t<bsocket_t> onSocket;
    event_t<>          onClose;
    event_t<except_t>  onError;
    event_t<bsocket_t> onOpen;
    
    /*─······································································─*/

    bth_t( decltype(NODE::func) _func, agent_t* opt=nullptr ) noexcept : obj( new NODE() ) 
         { obj->func=_func; obj->agent=opt==nullptr?agent_t():*opt; }
    
    /*─······································································─*/
    
    void     close() const noexcept { if( obj->state<=0 ){ return; } obj->state=-1; onClose.emit(); }
    
    bool is_closed() const noexcept { return obj == nullptr ? 1 : obj->state <= 0; }
    
    /*─······································································─*/

    void poll( bool chck ) const noexcept { obj->chck = chck; }
    
    /*─······································································─*/

    void listen( const string_t& host, int port, decltype(NODE::func)* cb=nullptr ) const noexcept {
        if( obj->state == 1 ){ return; } obj->state = 1; auto self = type::bind( this );

        auto sk = bsocket_t(); 
             sk.AF     = AF_BTH; 
             sk.SOCK   = SOCK_STREAM;
             sk.IPPROTO= IPPROTO_BTH;
             sk.socket( host, port ); 
             sk.set_sockopt( self->obj->agent ); 
        
        if( sk.bind()   < 0 ){ _EERROR(onError,"Error while binding Bluetooth");   close(); sk.free(); return; }
        if( sk.listen() < 0 ){ _EERROR(onError,"Error while listening Bluetooth"); close(); sk.free(); return; }
        if( obj->chck )      { init_poll_loop( self ); }

        onOpen.emit(sk); if( cb != nullptr ){ (*cb)(sk); } 
        
        process::task::add([=](){
            static int _accept = 0; 
        coStart

            while( !sk.is_closed() ){ _accept = sk._accept();
                if( self->is_closed() || !sk.is_available() )
                  { break; } elif ( _accept != -2 )
                  { break; } coYield(1);
            }

            if( _accept == -1 ){ _EERROR(self->onError,"Error while accepting Bluetooth"); coGoto(2); }
            elif ( !sk.is_available() || self->is_closed() ){ coGoto(2); }
            elif ( self->obj->chck ){ self->obj->poll.push_read(_accept); coGoto(0); }
            else { bsocket_t cli( _accept );
                   _poll_::poll task; cli.set_sockopt( self->obj->agent ); 
                   process::poll::add( task, cli, self, self->obj->func );
            coGoto(0); }

            coYield(2); self->close(); sk.free(); 
        
        coStop
        });

    }

    void listen( const string_t& host, int port, decltype(NODE::func) cb ) const noexcept { 
         listen( host, port, &cb ); 
    }
    
    /*─······································································─*/

    void connect( const string_t& host, int port, decltype(NODE::func)* fn=nullptr ) const noexcept {
        if( obj->state == 1 ){ return; }
        
        ptr_t<decltype( NODE::func )> cb = type::bind(fn);
        auto self = type::bind( this ); obj->state = 1;

        bsocket_t sk = bsocket_t(); 
                  sk.AF     = AF_BTH; 
                  sk.SOCK   = SOCK_STREAM;
                  sk.IPPROTO= IPPROTO_BTH;
                  sk.socket( host, port ); 
                  sk.set_sockopt( self->obj->agent );

        process::task::add([=](){
        coStart

            while( sk._connect() == -2 ){ coNext; } 
            if   ( sk._connect()  <  0 ){ 
                _EERROR(self->onError,"Error while connecting Bluetooth"); 
                self->close(); coEnd; 
            }

            if( self->obj->chck ){ 
                self->obj->poll.push_write( sk.get_fd() );
                while( self->obj->poll.get_last_poll()==nullptr )
                     { coNext; self->obj->poll.emit(); }
            }
            
            sk.onClose.once([=](){ self->close(); }); sk.onOpen.emit(); 
            self->onSocket.emit( sk );      self->onOpen.emit(sk); 
            if( cb != nullptr ){(*cb)(sk);} self->obj->func(sk);
            
        coStop
        });
        
    }

    void connect( const string_t& host, int port, decltype(NODE::func) cb ) const noexcept { 
         connect( host, port, &cb ); 
    }

};

/*────────────────────────────────────────────────────────────────────────────*/

namespace bth {

    bth_t server( const bth_t& server ){ server.onSocket([=]( bsocket_t cli ){
        cli.onDrain.once([=](){ cli.free(); cli.onData.clear(); });
        ptr_t<_file_::read> _read = new _file_::read;

        server.onConnect.once([=]( bsocket_t cli ){ process::poll::add([=](){
            if(!cli.is_available() )    { cli.close(); return -1; }
            if((*_read)(&cli)==1 )      { return 1; } 
            if(  _read->state<=0 )      { return 1; }
            cli.onData.emit(_read->data); return 1;
        }) ; });

        process::task::add([=](){
            server.onConnect.emit(cli); return -1;
        });

    }); return server; }

    /*─······································································─*/

    bth_t server( agent_t* opt=nullptr ){
        auto server = bth_t( [=]( bsocket_t /*unused*/ ){}, opt );
        bth::server( server ); return server; 
    }

    /*─······································································─*/

    bth_t client( const bth_t& client ){ client.onOpen.once([=]( bsocket_t cli ){
        cli.onDrain.once([=](){ cli.free(); cli.onData.clear(); });
        ptr_t<_file_::read> _read = new _file_::read;

        process::poll::add([=](){
            if(!cli.is_available() )    { cli.close(); return -1; }
            if((*_read)(&cli)==1 )      { return 1; } 
            if(  _read->state<=0 )      { return 1; }
            cli.onData.emit(_read->data); return 1;
        });

    }); return client; }

    /*─······································································─*/

    bth_t client( agent_t* opt=nullptr ){
        auto client = bth_t( [=]( bsocket_t /*unused*/ ){}, opt );
        bth::client( client ); return client; 
    }
    
}

/*────────────────────────────────────────────────────────────────────────────*/

}

#endif
