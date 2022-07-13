#include <chrono>
#include <thread>
#include <napi.h>
#include <stdio.h>

using namespace Napi;

std::thread nativeThread;
ThreadSafeFunction tsfn;

Value RegisterCallback( const CallbackInfo& info ){
    Napi::Env env = info.Env();
    if ( info.Length() < 1 ){
      throw Error::New( env, "usage: registerCallback(function)" );
    }
    else if ( !info[0].IsFunction() )
    {
      throw Error::New( env, "Expected first arg to be function" );
    }
    
    
    tsfn = ThreadSafeFunction::New(
        env,
        info[0].As<Function>(),  // JavaScript function called asynchronously
        "some loop",             // Name
        0,                       // Unlimited queue
        1,                       // Only one thread will use this initially
        []( Napi::Env ) {        // Finalizer used to clean threads up
          nativeThread.join();
        } );
    
    nativeThread = std::thread( [] {
      auto callback = []( Napi::Env env, Function jsCallback, int* value ) {
        // Transform native data into JS data, passing it to the provided
        // `jsCallback` -- the TSFN's JavaScript function.
        jsCallback.Call( {Number::New( env, *value )} );

        // We're finished with the data.
        delete value;
      };

      for ( int i = 0; i < 5; i++ )
      {
        // Create new data
        int* value = new int( clock() );

        // Perform a blocking call
        napi_status status = tsfn.BlockingCall( value, callback );
        if ( status != napi_ok )
        {
          // Handle error
          break;
        }

        std::this_thread::sleep_for( std::chrono::seconds( 5 ) );
      }

      // Release the thread-safe function
      tsfn.Release();
    } );
    return Boolean::New(env, true);
}

Object Init(Env env, Object exports) {
    exports.Set( "registerCallback", Function::New( env, RegisterCallback ) );
    return exports;
}

NODE_API_MODULE(testaddon, Init)
