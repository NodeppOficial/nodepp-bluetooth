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

    int next() const noexcept {
          if( obj->poll.emit()==-1 ){ return -1; } auto x = obj->poll.get_last_poll();
          if( x[0] == 0 ){ bsocket_t cli(x[1]); cli.set_sockopt( obj->agent ); onSocket.emit(cli); obj->func(cli); }
        elif( x[0] == 1 ){ bsocket_t cli(x[1]); cli.set_sockopt( obj->agent ); onSocket.emit(cli); obj->func(cli); }
        else             { bsocket_t cli(x[1]); cli.free(); } return 1;
    }
    
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

    void listen( const string_t& host, int port, decltype(NODE::func) cb ) const noexcept {
        if( obj->state == 1 ){ return; } auto self = type::bind( this ); obj->state = 1;

        auto sk = bsocket_t(); 
             sk.AF     = AF_BTH; 
             sk.SOCK   = SOCK_STREAM;
             sk.IPPROTO= IPPROTO_BTH;
             sk.socket( host, port ); 
             sk.set_sockopt( self->obj->agent ); 
        
        if( sk.bind()   < 0 ){ _EERROR(onError,"Error while binding Bluetooth");   close(); sk.free(); return; }
        if( sk.listen() < 0 ){ _EERROR(onError,"Error while listening Bluetooth"); close(); sk.free(); return; }
        
        cb( sk ); onOpen.emit( sk ); process::task::add([=](){
            int _accept = -2; self->next();

            while( _accept == -2 ){
               if( sk.is_closed() || sk.is_closed() ){ goto ERROR; } 
               if((_accept=sk._accept()) != -2 ){ break; } return 1; 
            }
            
              if( _accept < 0 ){ _EERROR(self->onError,"Error while accepting Bluetooth"); goto ERROR; }
            elif( self->obj->chck ){ if( self->obj->poll.push_read(_accept)==0 )
                { bsocket_t cli( _accept ); cli.free(); } 
            } else {
                  bsocket_t cli( _accept );
                  cli.set_sockopt( self->obj->agent ); _poll_::poll task; 
                  process::poll::add( task, cli, self, self->obj->func );
            }     
            
            return 1; ERROR:; self->close(); sk.free(); return -1;
        
        });

    }

    void listen( const string_t& host, int port ) const noexcept { 
         listen( host, port, []( bsocket_t ){} ); 
    }
    
    /*─······································································─*/

    void connect( const string_t& host, int port, decltype(NODE::func) cb ) const noexcept {
        if( obj->state == 1 ){ return; } auto self = type::bind( this ); obj->state = 1;

        bsocket_t sk = bsocket_t(); 
                  sk.AF     = AF_BTH; 
                  sk.SOCK   = SOCK_STREAM;
                  sk.IPPROTO= IPPROTO_BTH;
                  sk.socket( host, port ); 
                  sk.set_sockopt( self->obj->agent );

        process::task::add([=](){
            if( self->is_closed() ){ return -1; }
        coStart

            while( sk._connect() == -2 ){ coNext; } 
            if   ( sk._connect()  <  0 ){ 
                _EERROR(self->onError,"Error while connecting Bluetooth"); 
                self->close(); coEnd; 
            }

            if( self->obj->chck ){
            if( self->obj->poll.push_write(sk.get_fd())==0 )
              { sk.free(); } while( self->obj->poll.emit()==0 ){ 
                   if( process::now() > sk.get_send_timeout() )
                     { coEnd; } coNext; }
            }   cb( sk );
            
            sk.onClose.once([=](){ self->close(); }); 
            self->onSocket.emit(sk); sk.onOpen.emit();      
            self->onOpen.emit(sk); self->obj->func(sk);
            
        coStop
        });
        
    }

    void connect( const string_t& host, int port ) const noexcept { 
         connect( host, port, []( bsocket_t ){} ); 
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
